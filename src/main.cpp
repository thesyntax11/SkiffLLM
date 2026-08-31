#include "skifflm/config.hpp"
#include "skifflm/engine.hpp"
#include "skifflm/session.hpp"
#include "skifflm/terminal.hpp"

#include <algorithm>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <llama.h>

namespace {

#ifndef SKIFFLLM_VERSION
#define SKIFFLLM_VERSION "1.2.0"
#endif
const char* kVersion = SKIFFLLM_VERSION;
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

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buffer[8] = {0};
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    out << buffer;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    out << '"';
    return out.str();
}

bool write_text_file(const std::filesystem::path& path,
                     const std::string& content,
                     std::string& error) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create output directory: " + parent.string();
            return false;
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot open output file for writing: " + path.string();
        return false;
    }
    output << content;
    if (!output) {
        error = "failed to write output file: " + path.string();
        return false;
    }
    return true;
}

int print_models(const skifflm::Config& cfg) {
    std::string error;
    std::vector<std::filesystem::path> models = skifflm::discover_models(cfg, error);
    if (cfg.json_output) {
        std::cout << "[";
        for (size_t i = 0; i < models.size(); ++i) {
            if (i != 0) {
                std::cout << ",";
            }
            std::cout << json_escape(models[i].string());
        }
        std::cout << "]\n";
    } else {
        for (const auto& model : models) {
            std::cout << model.string() << "\n";
        }
    }
    return 0;
}

bool choose_model(skifflm::Config& cfg,
                  std::unique_ptr<skifflm::LlmEngine>& engine,
                  std::string& error);

std::string platform_name() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#else
    return "Unknown";
#endif
}

int run_doctor(const skifflm::Config& cfg, const std::string& version) {
    std::error_code ec;
    const bool model_dir_exists = std::filesystem::exists(cfg.model_dir, ec);
    const bool model_exists = !cfg.model_path.empty() &&
                              std::filesystem::exists(cfg.model_path, ec);

    if (cfg.json_output) {
        std::cout << "{\n";
        std::cout << "  \"version\":" << json_escape(version) << ",\n";
        std::cout << "  \"platform\":" << json_escape(platform_name()) << ",\n";
        std::cout << "  \"threads\":" << std::thread::hardware_concurrency() << ",\n";
        std::cout << "  \"supports_mmap\":" << (llama_supports_mmap() ? "true" : "false") << ",\n";
        std::cout << "  \"supports_mlock\":" << (llama_supports_mlock() ? "true" : "false") << ",\n";
        std::cout << "  \"supports_gpu\":" << (llama_supports_gpu_offload() ? "true" : "false") << ",\n";
        std::cout << "  \"model_dir\":" << json_escape(cfg.model_dir.string()) << ",\n";
        std::cout << "  \"model_dir_present\":" << (model_dir_exists ? "true" : "false") << ",\n";
        std::cout << "  \"config_path\":" << json_escape(cfg.config_path.string()) << ",\n";
        std::cout << "  \"history_path\":" << json_escape(cfg.history_path.string()) << ",\n";
        std::cout << "  \"model_path\":" << json_escape(cfg.model_path.string()) << ",\n";
        std::cout << "  \"model_present\":" << (model_exists ? "true" : "false") << "\n";
        std::cout << "}\n";
        return 0;
    }

    std::cout << "SkiffLLM diagnostics " << version << "\n";
    std::cout << "  Platform:            " << platform_name() << "\n";
    std::cout << "  Hardware threads:    " << std::thread::hardware_concurrency() << "\n";
    std::cout << "  Supports mmap:       " << (llama_supports_mmap() ? "yes" : "no") << "\n";
    std::cout << "  Supports mlock:      " << (llama_supports_mlock() ? "yes" : "no") << "\n";
    std::cout << "  Supports GPU:        " << (llama_supports_gpu_offload() ? "yes" : "no") << "\n";
    std::cout << "  Model directory:     " << cfg.model_dir.string()
              << (model_dir_exists ? " (present)" : " (missing)") << "\n";
    std::cout << "  Config path:         " << cfg.config_path.string() << "\n";
    std::cout << "  History path:        " << cfg.history_path.string() << "\n";
    std::cout << "  Model configured:    " << (cfg.model_path.empty()
                                                   ? std::string("(auto-discovery)")
                                                   : cfg.model_path.string())
              << (model_exists ? " (present)" : "") << "\n";
    return 0;
}

int print_model_info(skifflm::Config& cfg,
                     std::unique_ptr<skifflm::LlmEngine>& engine,
                     const skifflm::Terminal& terminal) {
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        if (cfg.json_output) {
            std::cout << "{\"error\":" << json_escape(model_error) << "}\n";
        } else {
            terminal.error(model_error);
        }
        return 1;
    }
    const auto& info = engine->info();
    if (cfg.json_output) {
        std::cout << "{\n";
        std::cout << "  \"model\":" << json_escape(cfg.model_path.string()) << ",\n";
        std::cout << "  \"description\":" << json_escape(info.description) << ",\n";
        std::cout << "  \"file_type\":" << json_escape(info.file_type) << ",\n";
        std::cout << "  \"params\":" << info.n_params << ",\n";
        std::cout << "  \"size_bytes\":" << info.size_bytes << ",\n";
        std::cout << "  \"context_train\":" << info.n_ctx_train << ",\n";
        std::cout << "  \"vocab_size\":" << info.n_vocab << "\n";
        std::cout << "}\n";
    } else {
        terminal.highlight("Model");
        terminal.write("  Path: " + cfg.model_path.string() + "\n");
        terminal.write("  Description: " + info.description + "\n");
        terminal.write("  Quantization: " + info.file_type + "\n");
        terminal.write("  Parameters: " + skifflm::to_human_count(info.n_params) + "\n");
        terminal.write("  File size: " + skifflm::to_human_bytes(info.size_bytes) + "\n");
        terminal.write("  Training context: " + std::to_string(info.n_ctx_train) + "\n");
        terminal.write("  Vocabulary: " + std::to_string(info.n_vocab) + "\n");
    }
    return 0;
}

int run_tokenize(skifflm::Config& cfg,
                 std::unique_ptr<skifflm::LlmEngine>& engine,
                 const skifflm::Terminal& terminal,
                 const std::string& text) {
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        if (cfg.json_output) {
            std::cout << "{\"error\":" << json_escape(model_error) << "}\n";
        } else {
            terminal.error(model_error);
        }
        return 1;
    }
    std::vector<int32_t> tokens;
    std::string error;
    if (!engine->tokenize(text, tokens, error)) {
        if (cfg.json_output) {
            std::cout << "{\"error\":" << json_escape(error) << "}\n";
        } else {
            terminal.error(error);
        }
        return 1;
    }
    if (cfg.json_output) {
        std::cout << "{\"text\":" << json_escape(text)
                  << ",\"tokens\":" << tokens.size() << "}\n";
    } else {
        terminal.write("Token count: ", skifflm::Color::Cyan);
        terminal.write(std::to_string(tokens.size()) + "\n");
        terminal.write("Token ids: ", skifflm::Color::Cyan);
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (i != 0) {
                terminal.write(" ");
            }
            terminal.write_raw(std::to_string(tokens[i]));
        }
        terminal.write("\n");
    }
    return 0;
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
                  std::string& error) {
    error.clear();
    if (engine) {
        return true;
    }

    std::vector<std::filesystem::path> models = skifflm::discover_models(cfg, error);
    if (!error.empty() && models.empty()) {
        error += "\nPass a GGUF model with --model or place one in the model directory.";
        return false;
    }

    if (models.size() > 1) {
        error = "Multiple GGUF models found. Choose one with --model:";
        for (const auto& path : models) {
            error += "\n  " + path.string();
        }
        return false;
    }

    cfg.model_path = models.front();
    engine = std::make_unique<skifflm::LlmEngine>(cfg);
    if (!engine->load(error)) {
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
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        terminal.error(model_error);
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
            if (command == "/settings") {
                terminal.write("Temperature: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.temperature) + "\n");
                terminal.write("Top-p: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.top_p) + "\n");
                terminal.write("Top-k: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.top_k) + "\n");
                terminal.write("Min-p: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.min_p) + "\n");
                terminal.write("Typical-p: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.typical_p) + "\n");
                terminal.write("Max tokens: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(options.n_predict) + "\n");
                terminal.write("Context: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(cfg.context_size) + "\n");
                terminal.write("Stop sequences: ", skifflm::Color::Cyan);
                if (options.stop_sequences.empty()) {
                    terminal.write_raw("(none)\n");
                } else {
                    for (size_t i = 0; i < options.stop_sequences.size(); ++i) {
                        if (i != 0) {
                            terminal.write_raw(", ");
                        }
                        terminal.write_raw(options.stop_sequences[i]);
                    }
                    terminal.write_raw("\n");
                }
                continue;
            }
            if (command == "/tokenize") {
                if (argument.empty()) {
                    terminal.error("Usage: /tokenize <text>");
                    continue;
                }
                std::vector<int32_t> tokens;
                std::string tokenize_error;
                if (!engine->tokenize(argument, tokens, tokenize_error)) {
                    terminal.error(tokenize_error);
                    continue;
                }
                terminal.write("Token count: ", skifflm::Color::Cyan);
                terminal.write_raw(std::to_string(tokens.size()) + "\n");
                terminal.write("Token ids: ", skifflm::Color::Cyan);
                for (size_t i = 0; i < tokens.size(); ++i) {
                    if (i != 0) {
                        terminal.write_raw(" ");
                    }
                    terminal.write_raw(std::to_string(tokens[i]));
                }
                terminal.write_raw("\n");
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
            if (command == "/profile") {
                if (argument.empty()) {
                    terminal.error("Usage: /profile <balanced|fast|creative|code|precise>");
                    continue;
                }
                std::string profile_error;
                if (!skifflm::apply_profile(cfg, argument, profile_error)) {
                    terminal.error(profile_error);
                    continue;
                }
                options.n_predict = cfg.n_predict;
                options.temperature = cfg.temperature;
                options.top_p = cfg.top_p;
                options.min_p = cfg.min_p;
                options.typical_p = cfg.typical_p;
                options.top_k = cfg.top_k;
                options.repeat_penalty = cfg.repeat_penalty;
                options.repeat_last_n = cfg.repeat_last_n;
                terminal.success("Profile set to " + cfg.profile_name);
                continue;
            }
            if (command == "/stop") {
                if (argument.empty()) {
                    if (options.stop_sequences.empty()) {
                        terminal.write("No stop sequences configured.\n");
                    } else {
                        terminal.write("Stop sequences: ", skifflm::Color::Cyan);
                        for (size_t i = 0; i < options.stop_sequences.size(); ++i) {
                            if (i != 0) {
                                terminal.write(", ");
                            }
                            terminal.write(options.stop_sequences[i]);
                        }
                        terminal.write("\n");
                    }
                    continue;
                }
                options.stop_sequences.push_back(argument);
                terminal.success("Stop sequence added: " + argument);
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
            if (command == "/min-p") {
                bool ok = false;
                const double value = parse_double(argument, ok);
                if (!ok || value < 0.0 || value > 1.0) {
                    terminal.error("Usage: /min-p <value in [0,1]>");
                    continue;
                }
                options.min_p = static_cast<float>(value);
                terminal.success("Min-p set to " + std::to_string(options.min_p));
                continue;
            }
            if (command == "/typical") {
                bool ok = false;
                const double value = parse_double(argument, ok);
                if (!ok || value < 0.0 || value > 1.0) {
                    terminal.error("Usage: /typical <value in [0,1]>");
                    continue;
                }
                options.typical_p = static_cast<float>(value);
                terminal.success("Typical-p set to " + std::to_string(options.typical_p));
                continue;
            }
            if (command == "/ctx") {
                bool ok = false;
                const int value = parse_int(argument, ok);
                if (!ok || value < 64) {
                    terminal.error("Usage: /ctx <context size >= 64>");
                    continue;
                }
                const int previous_ctx = cfg.context_size;
                cfg.context_size = value;
                std::string ctx_error;
                auto candidate = std::make_unique<skifflm::LlmEngine>(cfg);
                if (!candidate->load(ctx_error)) {
                    cfg.context_size = previous_ctx;
                    terminal.error(ctx_error);
                    continue;
                }
                engine = std::move(candidate);
                terminal.success("Context set to " + std::to_string(value));
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
            if (command == "/export") {
                if (argument.empty()) {
                    terminal.error("Usage: /export <path>");
                    continue;
                }
                std::ostringstream markdown;
                markdown << "# SkiffLLM Conversation\n\n";
                const auto conversation = session.conversation();
                for (const auto& message : conversation) {
                    const std::string role = message.role == "system" ? "System"
                                             : message.role == "user" ? "User"
                                             : message.role == "assistant" ? "Assistant"
                                             : message.role;
                    markdown << "## " << role << "\n\n" << message.content << "\n\n";
                }
                std::string export_error;
                if (!write_text_file(argument, markdown.str(), export_error)) {
                    terminal.error(export_error);
                } else {
                    terminal.success("Conversation exported to " + argument);
                }
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
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        if (cfg.json_output) {
            std::ostringstream out;
            out << "{\"error\":" << json_escape(model_error) << "}\n";
            std::cout << out.str();
        } else {
            terminal.error(model_error);
        }
        return 1;
    }

    std::vector<skifflm::ChatMessage> ask = session.conversation();
    ask.push_back({"user", prompt});

    skifflm::GenerationOptions current = options;
    if (!cfg.json_output) {
        current.token_callback = [&terminal](const std::string& part) {
            terminal.write_raw(part);
            std::cout.flush();
        };
        terminal.write("User: " + prompt + "\n", skifflm::Color::Cyan);
    }

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
        if (cfg.json_output) {
            std::ostringstream out;
            out << "{\"error\":" << json_escape(error) << "}\n";
            std::cout << out.str();
        } else {
            terminal.error(error);
        }
        return 1;
    }

    if (cfg.json_output) {
        std::ostringstream out;
        out << "{\n";
        out << "  \"text\":" << json_escape(result.text) << ",\n";
        out << "  \"model\":" << json_escape(cfg.model_path.string()) << ",\n";
        out << "  \"prompt_tokens\":" << result.prompt_tokens << ",\n";
        out << "  \"generated_tokens\":" << result.generated_tokens << ",\n";
        out << "  \"prompt_ms\":" << result.prompt_ms << ",\n";
        out << "  \"generation_ms\":" << result.generation_ms << ",\n";
        out << "  \"tokens_per_second\":" << result.tokens_per_second << ",\n";
        out << "  \"stopped\":" << (result.stopped ? "true" : "false") << "\n";
        out << "}\n";
        std::cout << out.str();
    } else {
        if (!result.text.empty() && result.text.back() != '\n') {
            terminal.write_raw("\n");
        }
        terminal.print_stats(result, "Generated");
    }

    if (!cfg.output_path.empty()) {
        std::string write_error;
        if (!write_text_file(cfg.output_path, result.text, write_error)) {
            terminal.error(write_error);
        } else if (!cfg.json_output) {
            terminal.success("Answer written to " + cfg.output_path.string());
        }
    }

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

    std::string config_path = cfg.config_path.string();
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
        std::cout << "SkiffLLM " << kVersion << " (llama.cpp " << llama_version() << ")\n";
        return 0;
    }
    if (cfg.show_config) {
        skifflm::print_config(cfg);
        return 0;
    }

    llama_backend_init();
    if (cfg.log_llama) {
        llama_log_set(log_to_stderr, nullptr);
    } else {
        llama_log_set(discard_log, nullptr);
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGQUIT, signal_handler);
#endif

    skifflm::Terminal terminal(cfg);
    std::unique_ptr<skifflm::LlmEngine> engine;

    if (cfg.list_models) {
        const int status = print_models(cfg);
        llama_backend_free();
        return status;
    }

    if (cfg.doctor) {
        const int status = run_doctor(cfg, kVersion);
        llama_backend_free();
        return status;
    }

    if (cfg.json_output) {
        cfg.interactive = false;
    }

    if (cfg.model_path.empty() && cfg.model_dir.empty()) {
        std::cerr << "No model configured. Use --model <path.gguf>.\n";
        return 2;
    }

    const std::filesystem::path model_path = cfg.model_path.empty()
                                                 ? std::filesystem::path()
                                                 : skifflm::expand_path(cfg.model_path.string());
    cfg.model_path = model_path;

    if (cfg.model_info) {
        const int status = print_model_info(cfg, engine, terminal);
        engine.reset();
        llama_backend_free();
        return status;
    }

    if (!cfg.tokenize_text.empty()) {
        const int status = run_tokenize(cfg, engine, terminal, cfg.tokenize_text);
        engine.reset();
        llama_backend_free();
        return status;
    }

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

    skifflm::GenerationOptions options;
    options.n_predict = cfg.n_predict;
    options.temperature = cfg.temperature;
    options.top_p = cfg.top_p;
    options.min_p = cfg.min_p;
    options.typical_p = cfg.typical_p;
    options.top_k = cfg.top_k;
    options.repeat_penalty = cfg.repeat_penalty;
    options.repeat_last_n = cfg.repeat_last_n;
    options.seed = cfg.seed;
    options.auto_trim = cfg.auto_trim;
    options.reserve_ctx = cfg.reserve_ctx;
    options.n_keep = cfg.n_keep;
    options.stop_sequences = cfg.stop_sequences;
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
