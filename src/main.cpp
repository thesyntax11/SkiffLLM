#include "skifflm/config.hpp"
#include "skifflm/engine.hpp"
#include "skifflm/session.hpp"
#include "skifflm/terminal.hpp"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include <llama.h>

namespace {

const char* kVersion = "1.0.0";
volatile std::sig_atomic_t g_interrupted = 0;

void signal_handler(int) {
    g_interrupted = 1;
}

void log_to_stderr(enum ggml_log_level, const char* text, void*) {
    std::cerr << text;
}

void discard_log(enum ggml_log_level, const char*, void*) {
}

double parse_double(const std::string& text, bool& ok) {
    if (text.empty()) {
        ok = false;
        return 0.0;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        ok = false;
        return 0.0;
    }
    ok = true;
    return value;
}

int parse_int(const std::string& text, bool& ok) {
    if (text.empty()) {
        ok = false;
        return 0;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        ok = false;
        return 0;
    }
    ok = true;
    return static_cast<int>(value);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string read_stdin() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

std::string strip_command(const std::string& command) {
    const size_t first = command.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }
    const size_t second = command.find_first_of(" \t", first);
    if (second == std::string::npos) {
        return command.substr(first);
    }
    return command.substr(first, second - first);
}

std::string command_argument(const std::string& command) {
    const std::string stripped = command.substr(command.find_first_not_of(" \t"));
    const size_t space = stripped.find_first_of(" \t");
    if (space == std::string::npos) {
        return {};
    }
    return stripped.substr(space + 1);
}

bool choose_model(skifflm::Config& cfg,
                  std::unique_ptr<skifflm::LlmEngine>& engine,
                  const skifflm::Terminal& terminal) {
    if (engine) {
        return true;
    }

    std::string error;
    std::vector<std::filesystem::path> models = skifflm::discover_models(cfg, error);
    if (!error.empty() && models.empty()) {
        terminal.error(error);
        terminal.error("Pass a GGUF model with --model or place one in the model directory.");
        return false;
    }

    if (models.size() > 1) {
        terminal.warning("Multiple GGUF models found. Choose one with --model:");
        for (const auto& path : models) {
            terminal.write("  " + path.string() + "\n");
        }
        return false;
    }

    cfg.model_path = models.front();
    engine = std::make_unique<skifflm::LlmEngine>(cfg);
    if (!engine->load(error)) {
        terminal.error(error);
        engine.reset();
        return false;
    }
    return true;
}

void print_session_info(const skifflm::LlmEngine& engine,
                        const skifflm::Session& session,
                        const skifflm::Terminal& terminal,
                        const skifflm::Config& cfg) {
    const auto& info = engine.info();
    terminal.highlight("Model");
    terminal.write("  Path: " + cfg.model_path.string() + "\n");
    if (!info.description.empty()) {
        terminal.write("  Description: " + info.description + "\n");
    }
    if (!info.file_type.empty()) {
        terminal.write("  Quantization: " + info.file_type + "\n");
    }
    terminal.write("  Parameters: " + skifflm::to_human_count(info.n_params) + "\n");
    terminal.write("  File size: " + skifflm::to_human_bytes(info.size_bytes) + "\n");
    terminal.write("  Training context: " + std::to_string(info.n_ctx_train) + "\n");
    terminal.write("  Vocabulary: " + std::to_string(info.n_vocab) + "\n");

    terminal.highlight("Session");
    terminal.write("  Messages: " + std::to_string(session.message_count()) + "\n");
    terminal.write("  System prompt: " + (session.system_prompt().empty()
                                              ? std::string("(none)")
                                              : session.system_prompt()) + "\n");
    terminal.write("  History file: " + cfg.history_path.string() + "\n");
    terminal.write("  Context: " + std::to_string(cfg.context_size) +
                   " | Batch: " + std::to_string(cfg.batch_size) + "\n");
}

bool rebuild_engine(skifflm::Config& cfg,
                    std::unique_ptr<skifflm::LlmEngine>& engine,
                    const std::string& model_path,
                    const skifflm::Terminal& terminal) {
    const std::string previous = cfg.model_path.string();
    cfg.model_path = std::filesystem::path(model_path);

    auto candidate = std::make_unique<skifflm::LlmEngine>(cfg);
    std::string error;
    if (!candidate->load(error)) {
        cfg.model_path = previous;
        terminal.error(error);
        return false;
    }

    engine = std::move(candidate);
    terminal.success("Model reloaded: " + cfg.model_path.string());
    return true;
}

bool ask_model(skifflm::LlmEngine& engine,
               const skifflm::GenerationOptions& options,
               const std::vector<skifflm::ChatMessage>& messages,
               skifflm::GenerationResult& result,
               const skifflm::Terminal& terminal) {
    std::string error;
    const bool ok = engine.generate(messages,
                                    options,
                                    result,
                                    []() {
                                        return g_interrupted != 0;
                                    },
                                    error);
    if (!ok) {
        terminal.error(error);
        return false;
    }
    if (!result.text.empty() && result.text.back() != '\n') {
        terminal.write_raw("\n");
    }
    terminal.print_stats(result, "Generated");
    return true;
}

int run_interactive(skifflm::Config& cfg,
                    std::unique_ptr<skifflm::LlmEngine>& engine,
                    skifflm::Session& session,
                    skifflm::Terminal& terminal,
                    skifflm::GenerationOptions& options) {
    if (!choose_model(cfg, engine, terminal)) {
        return 1;
    }

    if (cfg.show_info) {
        terminal.print_banner(kVersion, engine->info(), cfg);
    }

    while (true) {
        std::string input;
        const bool ok = terminal.read_prompt("you> ", input);
        if (!ok) {
            if (g_interrupted != 0) {
                g_interrupted = 0;
                terminal.warning("Interrupted.");
                continue;
            }
            break;
        }

        g_interrupted = 0;

        if (!input.empty() && input.front() == '/') {
            const std::string command = strip_command(input);
            const std::string argument = command_argument(input);

            if (command == "/exit" || command == "/quit") {
                break;
            }
            if (command == "/help") {
                terminal.print_help();
                continue;
            }
            if (command == "/info") {
                print_session_info(*engine, session, terminal, cfg);
                continue;
            }
            if (command == "/history") {
                terminal.print_history(session.conversation());
                continue;
            }
            if (command == "/clear") {
                session.messages().clear();
                std::string error;
                if (!session.save(error)) {
                    terminal.error(error);
                }
                terminal.success("Conversation cleared.");
                continue;
            }
            if (command == "/reset") {
                session.set_system_prompt(cfg.system_prompt);
                session.messages().clear();
                std::string error;
                if (!session.save(error)) {
                    terminal.error(error);
                }
                terminal.success("Session reset.");
                continue;
            }
            if (command == "/system") {
                if (argument.empty()) {
                    terminal.write("System prompt: " + (session.system_prompt().empty()
                                                            ? std::string("(none)")
                                                            : session.system_prompt()) + "\n");
                } else {
                    session.set_system_prompt(argument);
                    terminal.success("System prompt updated.");
                }
                continue;
            }
            if (command == "/model") {
                if (argument.empty()) {
                    terminal.error("Usage: /model <path>");
                    continue;
                }
                rebuild_engine(cfg, engine, argument, terminal);
                continue;
            }
            if (command == "/temp" || command == "/temperature") {
                bool ok = false;
                const double value = parse_double(argument, ok);
                if (!ok || value < 0.0) {
                    terminal.error("Usage: /temp <value>");
                    continue;
                }
                options.temperature = static_cast<float>(value);
                terminal.success("Temperature set to " + std::to_string(options.temperature));
                continue;
            }
            if (command == "/top-p") {
                bool ok = false;
                const double value = parse_double(argument, ok);
                if (!ok || value <= 0.0 || value > 1.0) {
                    terminal.error("Usage: /top-p <value in (0,1]>");
                    continue;
                }
                options.top_p = static_cast<float>(value);
                terminal.success("Top-p set to " + std::to_string(options.top_p));
                continue;
            }
            if (command == "/top-k") {
                bool ok = false;
                const int value = parse_int(argument, ok);
                if (!ok || value < 0) {
                    terminal.error("Usage: /top-k <non-negative integer>");
                    continue;
                }
                options.top_k = value;
                terminal.success("Top-k set to " + std::to_string(options.top_k));
                continue;
            }
            if (command == "/n" || command == "/n-predict") {
                bool ok = false;
                const int value = parse_int(argument, ok);
                if (!ok || value < 1) {
                    terminal.error("Usage: /n <tokens>");
                    continue;
                }
                options.n_predict = value;
                terminal.success("Max generated tokens set to " + std::to_string(options.n_predict));
                continue;
            }
            if (command == "/save") {
                std::string error;
                if (!session.save(error)) {
                    terminal.error(error);
                } else {
                    terminal.success("History saved to " + cfg.history_path.string());
                }
                continue;
            }

            terminal.error("Unknown command: " + command);
            terminal.error("Type /help for the command list.");
            continue;
        }

        if (input.empty()) {
            terminal.warning("Type /help for commands or enter a message.");
            continue;
        }

        session.messages().push_back({"user", input});

        std::vector<skifflm::ChatMessage> ask = session.conversation();
        skifflm::GenerationOptions current = options;
        current.token_callback = [&terminal](const std::string& part) {
            terminal.write_raw(part);
            std::cout.flush();
        };

        skifflm::GenerationResult result;
        const bool generate_ok = ask_model(*engine,
                                           current,
                                           ask,
                                           result,
                                           terminal);
        if (!generate_ok) {
            session.messages().pop_back();
            continue;
        }

        session.messages().push_back({"assistant", result.text});
        if (cfg.save_history) {
            std::string error;
            if (!session.save(error)) {
                terminal.error(error);
            }
        }
    }

    if (cfg.save_history) {
        std::string error;
        if (!session.save(error)) {
            terminal.error(error);
        }
    }

    terminal.info("Goodbye.");
    return 0;
}

int run_one_shot(skifflm::Config& cfg,
                 std::unique_ptr<skifflm::LlmEngine>& engine,
                 skifflm::Session& session,
                 skifflm::Terminal& terminal,
                 const skifflm::GenerationOptions& options,
                 const std::string& prompt) {
    if (!choose_model(cfg, engine, terminal)) {
        return 1;
    }

    std::vector<skifflm::ChatMessage> ask = session.conversation();
    ask.push_back({"user", prompt});

    skifflm::GenerationOptions current = options;
    current.token_callback = [&terminal](const std::string& part) {
        terminal.write_raw(part);
        std::cout.flush();
    };

    terminal.write("User: " + prompt + "\n", skifflm::Color::Cyan);
    skifflm::GenerationResult result;

    std::string error;
    const bool ok = engine->generate(ask,
                                     current,
                                     result,
                                     []() {
                                         return g_interrupted != 0;
                                     },
                                     error);
    if (!ok) {
        terminal.error(error);
        return 1;
    }
    if (!result.text.empty() && result.text.back() != '\n') {
        terminal.write_raw("\n");
    }
    terminal.print_stats(result, "Generated");

    session.messages().push_back({"user", prompt});
    session.messages().push_back({"assistant", result.text});
    if (cfg.save_history) {
        std::string save_error;
        if (!session.save(save_error)) {
            terminal.error(save_error);
        }
    }
    return 0;
}

}

int main(int argc, char** argv) {
    skifflm::Config cfg = skifflm::default_config();
    skifflm::apply_environment(cfg);

    std::string config_path = "";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.compare(0, 9, "--config=") == 0) {
            config_path = arg.substr(9);
            break;
        }
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }
    if (!config_path.empty()) {
        cfg.config_path = skifflm::expand_path(config_path);
    }

    std::string error;
    if (!skifflm::parse_config_file(cfg.config_path, cfg, error)) {
        std::cerr << error << std::endl;
        return 1;
    }
    if (!skifflm::parse_args(argc, argv, cfg, error)) {
        std::cerr << error << std::endl;
        return 2;
    }

    if (cfg.debug) {
        cfg.log_llama = true;
    }

    if (cfg.show_help) {
        std::cout << skifflm::usage(argv[0]);
        return 0;
    }
    if (cfg.show_version) {
        std::cout << "SkiffLLM " << kVersion << "\n";
        return 0;
    }
    if (cfg.show_config) {
        skifflm::print_config(cfg);
        return 0;
    }

    if (cfg.model_path.empty() && cfg.model_dir.empty()) {
        std::cerr << "No model configured. Use --model <path.gguf>.\n";
        return 2;
    }

    const std::filesystem::path model_path = cfg.model_path.empty()
                                                 ? std::filesystem::path()
                                                 : skifflm::expand_path(cfg.model_path.string());
    cfg.model_path = model_path;

    llama_backend_init();
    if (cfg.log_llama) {
        llama_log_set(log_to_stderr, nullptr);
    } else {
        llama_log_set(discard_log, nullptr);
    }

    std::signal(SIGINT, signal_handler);

    skifflm::Terminal terminal(cfg);

    if (cfg.reset_history) {
        cfg.save_history = true;
    }

    skifflm::Session session(cfg);
    session.set_system_prompt(cfg.system_prompt);
    error.clear();
    if (!session.load(error)) {
        terminal.error(error);
        llama_backend_free();
        return 1;
    }

    std::unique_ptr<skifflm::LlmEngine> engine;

    skifflm::GenerationOptions options;
    options.n_predict = cfg.n_predict;
    options.temperature = cfg.temperature;
    options.top_p = cfg.top_p;
    options.top_k = cfg.top_k;
    options.repeat_penalty = cfg.repeat_penalty;
    options.repeat_last_n = cfg.repeat_last_n;
    options.seed = cfg.seed;
    options.token_callback = nullptr;

    if (!cfg.interactive && !cfg.one_shot.empty()) {
        const int status = run_one_shot(cfg, engine, session, terminal, options, cfg.one_shot);
        engine.reset();
        llama_backend_free();
        return status;
    }

    if (!cfg.interactive && !cfg.prompt_file.empty()) {
        const std::string prompt = read_file(cfg.prompt_file);
        if (prompt.empty()) {
            terminal.error("Prompt file is empty or unreadable: " + cfg.prompt_file.string());
            engine.reset();
            llama_backend_free();
            return 1;
        }
        const int status = run_one_shot(cfg, engine, session, terminal, options, prompt);
        engine.reset();
        llama_backend_free();
        return status;
    }

    if (!cfg.interactive && cfg.read_stdin) {
        const std::string prompt = read_stdin();
        if (prompt.empty()) {
            terminal.error("No prompt received from stdin.");
            engine.reset();
            llama_backend_free();
            return 1;
        }
        const int status = run_one_shot(cfg, engine, session, terminal, options, prompt);
        engine.reset();
        llama_backend_free();
        return status;
    }

    if (!cfg.interactive) {
        const std::string prompt = read_stdin();
        if (prompt.empty()) {
            terminal.error("No prompt received. Use --prompt or pipe a prompt through stdin.");
            engine.reset();
            llama_backend_free();
            return 1;
        }
        const int status = run_one_shot(cfg, engine, session, terminal, options, prompt);
        engine.reset();
        llama_backend_free();
        return status;
    }

    const int status = run_interactive(cfg, engine, session, terminal, options);
    engine.reset();
    llama_backend_free();
    return status;
}
