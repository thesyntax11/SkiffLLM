#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#include "skiffllm/cli_utils.hpp"
#include "skiffllm/config.hpp"
#include "skiffllm/engine.hpp"
#include "skiffllm/server.hpp"
#include "skiffllm/session.hpp"
#include "skiffllm/terminal.hpp"
#include "skiffllm/tools.hpp"

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <llama.h>

namespace {

using skiffllm::cli::compact_preview;
using skiffllm::cli::expand_at_paths;
using skiffllm::cli::export_markdown;
using skiffllm::cli::json_escape;
using skiffllm::cli::make_attach_block;
using skiffllm::cli::parse_double;
using skiffllm::cli::parse_int;
using skiffllm::cli::read_file;
using skiffllm::cli::read_stdin;
using skiffllm::cli::write_text_file;

#ifndef SKIFFLLM_VERSION
#define SKIFFLLM_VERSION "1.6.0"
#endif
const char* kVersion = SKIFFLLM_VERSION;
volatile std::sig_atomic_t g_interrupted = 0;

void signal_handler(int) {
    g_interrupted = 1;
}

bool keep_open_on_no_arguments() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void wait_for_enter() {
#ifdef _WIN32
    int ch = _getch();
    while (ch != '\r' && ch != '\n' && ch != EOF) {
        ch = _getch();
    }
#else
    std::cin.get();
#endif
}

class KeepConsoleOpen {
   public:
    explicit KeepConsoleOpen(bool enabled) : enabled_(enabled) {}
    ~KeepConsoleOpen() {
        if (!enabled_) {
            return;
        }
        std::cout << "\nPress Enter to close...\n";
        std::cout.flush();
        wait_for_enter();
    }

   private:
    bool enabled_;
};

void log_to_stderr(enum ggml_log_level, const char* text, void*) {
    std::cerr << text;
}

void discard_log(enum ggml_log_level, const char*, void*) {}

int print_models(const skiffllm::Config& cfg) {
    std::string error;
    std::vector<std::filesystem::path> models = skiffllm::discover_models(cfg, error);
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

bool choose_model(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
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

int run_doctor(const skiffllm::Config& cfg, const std::string& version) {
    std::error_code ec;
    const bool model_dir_exists = std::filesystem::exists(cfg.model_dir, ec);
    const bool model_exists =
        !cfg.model_path.empty() && std::filesystem::exists(cfg.model_path, ec);

    if (cfg.json_output) {
        std::cout << "{\n";
        std::cout << "  \"version\":" << json_escape(version) << ",\n";
        std::cout << "  \"platform\":" << json_escape(platform_name()) << ",\n";
        std::cout << "  \"threads\":" << std::thread::hardware_concurrency() << ",\n";
        std::cout << "  \"supports_mmap\":" << (llama_supports_mmap() ? "true" : "false") << ",\n";
        std::cout << "  \"supports_mlock\":" << (llama_supports_mlock() ? "true" : "false")
                  << ",\n";
        std::cout << "  \"supports_gpu\":" << (llama_supports_gpu_offload() ? "true" : "false")
                  << ",\n";
        std::cout << "  \"model_dir\":" << json_escape(cfg.model_dir.string()) << ",\n";
        std::cout << "  \"model_dir_present\":" << (model_dir_exists ? "true" : "false") << ",\n";
        std::cout << "  \"config_path\":" << json_escape(cfg.config_path.string()) << ",\n";
        std::cout << "  \"history_path\":" << json_escape(cfg.history_path.string()) << ",\n";
        std::cout << "  \"model_path\":" << json_escape(cfg.model_path.string()) << ",\n";
        std::cout << "  \"model_present\":" << (model_exists ? "true" : "false") << ",\n";
        std::cout << "  \"core_inference_outbound\":\"none\",\n";
        std::cout << "  \"telemetry\":\"disabled\",\n";
        std::cout << "  \"cloud_api\":\"none\",\n";
        std::cout << "  \"history_storage\":\"local\",\n";
        std::cout << "  \"explicit_network_usages\":[\"model install (Hugging Face "
                     "download)\",\"openai subcommand (user-chosen endpoint)\"],\n";
        std::cout << "  \"privacy_status\":\"LOCAL-FIRST\"\n";
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
    std::cout << "  Model configured:    "
              << (cfg.model_path.empty() ? std::string("(auto-discovery)")
                                         : cfg.model_path.string())
              << (model_exists ? " (present)" : "") << "\n";
    if (cfg.doctor_network) {
        std::cout << "  Core inference outbound: none (generation never connects)\n";
        std::cout << "  Explicit network uses:   model install (Hugging Face),\n";
        std::cout << "                           openai subcommand (user-chosen endpoint)\n";
        std::cout << "  Telemetry:               disabled\n";
        std::cout << "  Cloud APIs:              none\n";
        std::cout << "  History storage:         local\n";
        std::cout << "  Privacy status:          ✓ LOCAL-FIRST\n";
    }
    return 0;
}

int print_model_info(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                     const skiffllm::Terminal& terminal) {
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
        terminal.write("  Parameters: " + skiffllm::to_human_count(info.n_params) + "\n");
        terminal.write("  File size: " + skiffllm::to_human_bytes(info.size_bytes) + "\n");
        terminal.write("  Training context: " + std::to_string(info.n_ctx_train) + "\n");
        terminal.write("  Vocabulary: " + std::to_string(info.n_vocab) + "\n");
    }
    return 0;
}

int run_tokenize(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                 const skiffllm::Terminal& terminal, const std::string& text) {
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
        std::cout << "{\"text\":" << json_escape(text) << ",\"tokens\":" << tokens.size() << "}\n";
    } else {
        terminal.write("Token count: ", skiffllm::Color::Cyan);
        terminal.write(std::to_string(tokens.size()) + "\n");
        terminal.write("Token ids: ", skiffllm::Color::Cyan);
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

bool choose_model(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                  std::string& error) {
    error.clear();
    if (engine) {
        return true;
    }

    std::vector<std::filesystem::path> models = skiffllm::discover_models(cfg, error);
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
    engine = std::make_unique<skiffllm::SkiffEngine>(cfg);
    if (!engine->load(error)) {
        engine.reset();
        return false;
    }
    return true;
}

void print_session_info(const skiffllm::SkiffEngine& engine, const skiffllm::Session& session,
                        const skiffllm::Terminal& terminal, const skiffllm::Config& cfg) {
    const auto& info = engine.info();
    terminal.highlight("Model");
    terminal.write("  Path: " + cfg.model_path.string() + "\n");
    if (!info.description.empty()) {
        terminal.write("  Description: " + info.description + "\n");
    }
    if (!info.file_type.empty()) {
        terminal.write("  Quantization: " + info.file_type + "\n");
    }
    terminal.write("  Parameters: " + skiffllm::to_human_count(info.n_params) + "\n");
    terminal.write("  File size: " + skiffllm::to_human_bytes(info.size_bytes) + "\n");
    terminal.write("  Training context: " + std::to_string(info.n_ctx_train) + "\n");
    terminal.write("  Vocabulary: " + std::to_string(info.n_vocab) + "\n");

    terminal.highlight("Session");
    terminal.write("  Messages: " + std::to_string(session.message_count()) + "\n");
    terminal.write(
        "  System prompt: " +
        (session.system_prompt().empty() ? std::string("(none)") : session.system_prompt()) + "\n");
    terminal.write("  History file: " + cfg.history_path.string() + "\n");
    terminal.write("  Context: " + std::to_string(cfg.context_size) +
                   " | Batch: " + std::to_string(cfg.batch_size) + "\n");
}

bool rebuild_engine(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                    const std::string& model_path, const skiffllm::Terminal& terminal) {
    const std::string previous = cfg.model_path.string();
    cfg.model_path = std::filesystem::path(model_path);

    auto candidate = std::make_unique<skiffllm::SkiffEngine>(cfg);
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

class LiveStreamer {
   public:
    explicit LiveStreamer(const skiffllm::Terminal& terminal)
        : terminal_(terminal), enabled_(terminal.live_output()) {}

    void reset() {
        buffer_.clear();
        tokens_ = 0;
        live_ = enabled_;
        width_ = terminal_.terminal_width();
    }

    void write(const std::string& part) {
        tokens_ += 1;
        if (!live_) {
            terminal_.write_raw(part);
            return;
        }
        buffer_ += part;
        if (buffer_.find('\n') != std::string::npos) {
            terminal_.write_raw_line(buffer_);
            buffer_.clear();
            live_ = false;
            return;
        }
        if (width_ > 0 && buffer_.size() + 8 >= static_cast<size_t>(width_)) {
            terminal_.finish_live();
            terminal_.write_raw(buffer_);
            buffer_.clear();
            live_ = false;
            return;
        }
        if (buffer_.size() > 120) {
            terminal_.finish_live();
            terminal_.write_raw(buffer_);
            buffer_.clear();
            live_ = false;
            return;
        }
        terminal_.write_live(buffer_, tokens_);
    }

    void finish() {
        if (live_) {
            terminal_.finish_live();
            buffer_.clear();
            live_ = false;
        }
    }

   private:
    const skiffllm::Terminal& terminal_;
    bool enabled_;
    bool live_ = false;
    int width_ = 0;
    std::string buffer_;
    std::size_t tokens_ = 0;
};

bool warmup_model(skiffllm::SkiffEngine& engine, const skiffllm::Terminal& terminal,
                  bool quiet = false) {
    std::string error;
    const auto start = std::chrono::steady_clock::now();
    const bool ok = engine.warmup(error);
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (!ok) {
        if (!quiet) {
            terminal.warning("Warmup failed: " + error);
        }
        return false;
    }
    if (!quiet) {
        terminal.success("Warmup complete in " + std::to_string(elapsed) + " ms.");
    }
    return true;
}

bool ask_model(skiffllm::SkiffEngine& engine, const skiffllm::GenerationOptions& options,
               const std::vector<skiffllm::ChatMessage>& messages,
               skiffllm::GenerationResult& result, const skiffllm::Terminal& terminal,
               LiveStreamer& streamer) {
    std::string error;
    streamer.reset();
    skiffllm::GenerationOptions current = options;
    current.token_callback = [&streamer](const std::string& part) { streamer.write(part); };
    const bool ok = engine.generate(
        messages, current, result, []() { return g_interrupted != 0; }, error);
    streamer.finish();
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

bool compact_session(const skiffllm::Config& cfg, skiffllm::SkiffEngine& engine,
                     skiffllm::Session& session, skiffllm::Terminal& terminal,
                     const skiffllm::GenerationOptions& options) {
    const auto conversation = session.conversation();
    if (conversation.size() < 2) {
        terminal.info("Nothing to compact yet.");
        return true;
    }

    std::ostringstream transcript;
    for (const auto& message : conversation) {
        if (message.role == "system") {
            continue;
        }
        transcript << message.role << ": " << message.content << "\n\n";
    }

    std::vector<skiffllm::ChatMessage> ask;
    ask.push_back({"system",
                   "You are a memory compression assistant. Preserve facts, "
                   "decisions, the user's preferences, file paths, and unfinished "
                   "work. Keep it compact and complete."});
    ask.push_back({"user",
                   "Compress this conversation into a compact bullet summary.\n\n"
                   "<conversation>\n" +
                       transcript.str() + "</conversation>\n"});

    skiffllm::GenerationOptions compact_options = options;
    compact_options.temperature = 0.2f;
    compact_options.n_predict = std::max(compact_options.n_predict, 512);
    compact_options.stop_sequences.clear();

    skiffllm::GenerationResult result;
    LiveStreamer streamer(terminal);
    streamer.reset();
    compact_options.token_callback = [&streamer](const std::string& part) { streamer.write(part); };
    terminal.write("Compacting conversation...\n", skiffllm::Color::Cyan);

    std::string error;
    const bool ok = engine.generate(
        ask, compact_options, result, []() { return g_interrupted != 0; }, error);
    streamer.finish();
    if (!ok) {
        terminal.error(error);
        return false;
    }
    if (result.text.empty()) {
        terminal.error("Compaction produced no output.");
        return false;
    }

    session.messages().clear();
    session.messages().push_back({"user", "[Compacted conversation]"});
    session.messages().push_back({"assistant", result.text});
    std::string save_error;
    if (!session.save(save_error)) {
        terminal.error(save_error);
        return false;
    }
    terminal.success("Conversation compacted and saved.");
    terminal.print_stats(result, "Compacted");
    skiffllm::record_generation(cfg, result.prompt_tokens, result.generated_tokens,
                                result.prompt_ms, result.generation_ms, result.tokens_per_second);
    return true;
}

bool regenerate_session(const skiffllm::Config& cfg, skiffllm::SkiffEngine& engine,
                        skiffllm::Session& session, skiffllm::Terminal& terminal,
                        const skiffllm::GenerationOptions& options) {
    auto& messages = session.messages();
    int last_user = -1;
    for (int i = static_cast<int>(messages.size()) - 1; i >= 0; --i) {
        if (messages[static_cast<size_t>(i)].role == "user") {
            last_user = i;
            break;
        }
    }
    if (last_user < 0) {
        terminal.info("No user message to regenerate.");
        return true;
    }
    while (static_cast<int>(messages.size()) > last_user + 1) {
        messages.pop_back();
    }

    std::vector<skiffllm::ChatMessage> ask = session.conversation();
    skiffllm::GenerationResult result;
    LiveStreamer streamer(terminal);
    streamer.reset();
    skiffllm::GenerationOptions regen_options = options;
    regen_options.token_callback = [&streamer](const std::string& part) { streamer.write(part); };
    terminal.write("Regenerating response...\n", skiffllm::Color::Cyan);

    std::string error;
    const bool ok = engine.generate(
        ask, regen_options, result, []() { return g_interrupted != 0; }, error);
    streamer.finish();
    if (!ok) {
        terminal.error(error);
        return false;
    }
    if (!result.text.empty() && result.text.back() != '\n') {
        terminal.write_raw("\n");
    }
    messages.push_back({"assistant", result.text});
    if (cfg.save_history) {
        std::string save_error;
        if (!session.save(save_error)) {
            terminal.error(save_error);
        }
    }
    terminal.print_stats(result, "Generated");
    skiffllm::record_generation(cfg, result.prompt_tokens, result.generated_tokens,
                                result.prompt_ms, result.generation_ms, result.tokens_per_second);
    return true;
}

int run_interactive(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                    skiffllm::Session& session, skiffllm::Terminal& terminal,
                    skiffllm::GenerationOptions& options) {
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        terminal.error(model_error);
        return 1;
    }

    if (cfg.show_info) {
        terminal.print_banner(kVersion, engine->info(), cfg);
    }

    if (cfg.warmup) {
        warmup_model(*engine, terminal);
    }

    std::vector<std::filesystem::path> attached = cfg.attach_paths;

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
            if (command == "/regenerate" || command == "/retry") {
                regenerate_session(cfg, *engine, session, terminal, options);
                continue;
            }
            if (command == "/warmup") {
                warmup_model(*engine, terminal);
                continue;
            }
            if (command == "/history") {
                terminal.print_history(session.conversation());
                continue;
            }
            if (command == "/compact") {
                compact_session(cfg, *engine, session, terminal, options);
                continue;
            }
            if (command == "/stats") {
                skiffllm::print_usage_stats(cfg, false);
                continue;
            }
            if (command == "/settings") {
                terminal.write("Temperature: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.temperature) + "\n");
                terminal.write("Top-p: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.top_p) + "\n");
                terminal.write("Top-k: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.top_k) + "\n");
                terminal.write("Min-p: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.min_p) + "\n");
                terminal.write("Typical-p: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.typical_p) + "\n");
                terminal.write("Max tokens: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(options.n_predict) + "\n");
                terminal.write("Context: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(cfg.context_size) + "\n");
                terminal.write("Stop sequences: ", skiffllm::Color::Cyan);
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
                terminal.write("Token count: ", skiffllm::Color::Cyan);
                terminal.write_raw(std::to_string(tokens.size()) + "\n");
                terminal.write("Token ids: ", skiffllm::Color::Cyan);
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
            if (command == "/remember" || command == "/memory") {
                if (argument.empty()) {
                    terminal.error("Usage: /remember <fact>");
                    continue;
                }
                std::string memory_error;
                if (!skiffllm::append_memory(cfg, argument, memory_error)) {
                    terminal.error(memory_error);
                } else {
                    terminal.success("Remembered.");
                }
                continue;
            }
            if (command == "/forget") {
                if (argument.empty()) {
                    terminal.error("Usage: /forget <text to remove>");
                    continue;
                }
                size_t removed = 0;
                std::string memory_error;
                if (!skiffllm::remove_memory(cfg, argument, removed, memory_error)) {
                    terminal.error(memory_error);
                } else {
                    terminal.success("Forgot " + std::to_string(removed) + " memory line(s).");
                }
                continue;
            }
            if (command == "/memories") {
                const std::string memory = skiffllm::load_memories(cfg);
                terminal.write("Memories:\n", skiffllm::Color::Cyan);
                terminal.write_raw(memory.empty() ? "(none)\n" : memory + "\n");
                continue;
            }
            if (command == "/clear-memories") {
                std::string memory_error;
                if (!skiffllm::clear_memories(cfg, memory_error)) {
                    terminal.error(memory_error);
                } else {
                    terminal.success("All memories cleared.");
                }
                continue;
            }
            if (command == "/system") {
                if (argument.empty()) {
                    terminal.write("System prompt: " +
                                   (session.system_prompt().empty() ? std::string("(none)")
                                                                    : session.system_prompt()) +
                                   "\n");
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
            if (command == "/file" || command == "/attach") {
                if (argument.empty()) {
                    terminal.error("Usage: /file <path>");
                    continue;
                }
                const std::filesystem::path path = skiffllm::expand_path(argument);
                std::error_code ec;
                if (!std::filesystem::exists(path, ec) ||
                    !std::filesystem::is_regular_file(path, ec)) {
                    terminal.error("Attached file does not exist: " + path.string());
                    continue;
                }
                attached.push_back(path);
                terminal.success("Attached " + path.string());
                continue;
            }
            if (command == "/clear-attach" || command == "/detach") {
                attached.clear();
                terminal.success("Attached files cleared.");
                continue;
            }
            if (command == "/profile") {
                if (argument.empty()) {
                    terminal.error("Usage: /profile <balanced|fast|creative|code|precise>");
                    continue;
                }
                std::string profile_error;
                if (!skiffllm::apply_profile(cfg, argument, profile_error)) {
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
                        terminal.write("Stop sequences: ", skiffllm::Color::Cyan);
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
                auto candidate = std::make_unique<skiffllm::SkiffEngine>(cfg);
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
                terminal.success("Max generated tokens set to " +
                                 std::to_string(options.n_predict));
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
                    const std::string role = message.role == "system"      ? "System"
                                             : message.role == "user"      ? "User"
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

        std::string effective_input = input;
        std::string attach_error;
        effective_input = expand_at_paths(effective_input, attach_error);
        if (!attach_error.empty()) {
            terminal.error(attach_error);
            continue;
        }
        if (!attached.empty()) {
            const std::string block = make_attach_block(attached, attach_error);
            if (!attach_error.empty()) {
                terminal.error(attach_error);
                continue;
            }
            effective_input = block + effective_input;
        }

        session.messages().push_back({"user", effective_input});

        std::vector<skiffllm::ChatMessage> ask = session.conversation();
        LiveStreamer streamer(terminal);

        skiffllm::GenerationResult result;
        const bool generate_ok = ask_model(*engine, options, ask, result, terminal, streamer);
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

        if (cfg.context_bar) {
            const uint32_t capacity = engine->context_capacity();
            const uint64_t used = static_cast<uint64_t>(result.prompt_tokens) +
                                  static_cast<uint64_t>(result.generated_tokens);
            terminal.print_context_bar(used, capacity);
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

struct BenchmarkRun {
    int prompt_tokens = 0;
    int generated_tokens = 0;
    double prompt_ms = 0.0;
    double generation_ms = 0.0;
    double tokens_per_second = 0.0;
};

int run_benchmark(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                  skiffllm::Terminal& terminal, const skiffllm::GenerationOptions& options) {
    std::string model_error;
    if (!choose_model(cfg, engine, model_error)) {
        if (cfg.json_output) {
            std::cout << "{\"error\":" << json_escape(model_error) << "}\n";
        } else {
            terminal.error(model_error);
        }
        return 1;
    }

    if (cfg.warmup) {
        warmup_model(*engine, terminal, cfg.json_output);
    }

    const std::string prompt_text = "Write a short poem about the sea.";
    std::vector<skiffllm::ChatMessage> ask;
    ask.push_back({"user", prompt_text});

    skiffllm::GenerationOptions run_options = options;
    run_options.seed = 42;
    run_options.n_predict = std::min(options.n_predict, 128);
    run_options.stop_sequences.clear();

    std::vector<BenchmarkRun> runs;
    runs.reserve(static_cast<size_t>(cfg.benchmark_runs));

    double worst_prompt_ms = 0.0;
    double best_generation_tps = 0.0;
    for (int i = 0; i < cfg.benchmark_runs; ++i) {
        skiffllm::GenerationResult result;
        run_options.token_callback = nullptr;
        std::string error;
        const bool ok = engine->generate(
            ask, run_options, result, []() { return g_interrupted != 0; }, error);
        if (!ok) {
            if (cfg.json_output) {
                std::cout << "{\"error\":" << json_escape(error) << "}\n";
            } else {
                terminal.error(error);
            }
            return 1;
        }
        BenchmarkRun entry;
        entry.prompt_tokens = result.prompt_tokens;
        entry.generated_tokens = result.generated_tokens;
        entry.prompt_ms = result.prompt_ms;
        entry.generation_ms = result.generation_ms;
        entry.tokens_per_second = result.tokens_per_second;
        runs.push_back(entry);
        worst_prompt_ms = std::max(worst_prompt_ms, result.prompt_ms);
        if (result.tokens_per_second > best_generation_tps) {
            best_generation_tps = result.tokens_per_second;
        }
    }

    double total_prompt_ms = 0.0;
    double total_generation_ms = 0.0;
    int total_generated = 0;
    for (const auto& run : runs) {
        total_prompt_ms += run.prompt_ms;
        total_generation_ms += run.generation_ms;
        total_generated += run.generated_tokens;
    }
    const double average_prompt_ms = total_prompt_ms / runs.size();
    const double average_generation_ms = total_generation_ms / runs.size();
    const double average_tokens = static_cast<double>(total_generated) / runs.size();
    const double average_tps =
        total_generation_ms > 0.0 ? (total_generated / (total_generation_ms / 1000.0)) : 0.0;

    if (cfg.json_output) {
        std::ostringstream out;
        out << "{\n";
        out << "  \"model\":" << json_escape(cfg.model_path.string()) << ",\n";
        out << "  \"prompt\":" << json_escape(prompt_text) << ",\n";
        out << "  \"runs\":" << cfg.benchmark_runs << ",\n";
        out << "  \"average_prompt_ms\":" << average_prompt_ms << ",\n";
        out << "  \"average_generation_ms\":" << average_generation_ms << ",\n";
        out << "  \"average_generated_tokens\":" << average_tokens << ",\n";
        out << "  \"average_tokens_per_second\":" << average_tps << ",\n";
        out << "  \"best_generation_tokens_per_second\":" << best_generation_tps << ",\n";
        out << "  \"worst_prompt_ms\":" << worst_prompt_ms << ",\n";
        out << "  \"runs_data\":[";
        for (size_t i = 0; i < runs.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << "{\"prompt_tokens\":" << runs[i].prompt_tokens
                << ",\"generated_tokens\":" << runs[i].generated_tokens
                << ",\"prompt_ms\":" << runs[i].prompt_ms
                << ",\"generation_ms\":" << runs[i].generation_ms
                << ",\"tokens_per_second\":" << runs[i].tokens_per_second << "}";
        }
        out << "]\n}\n";
        std::cout << out.str();
    } else {
        terminal.highlight("SkiffLLM benchmark");
        terminal.write("  Model: " + cfg.model_path.string() + "\n");
        terminal.write("  Prompt: " + prompt_text + "\n");
        terminal.write("  Runs: " + std::to_string(cfg.benchmark_runs) + "\n");
        terminal.write("  Average prompt time: " + std::to_string(average_prompt_ms) + " ms\n");
        terminal.write("  Average generation time: " + std::to_string(average_generation_ms) +
                       " ms\n");
        terminal.write("  Average generated tokens: " + std::to_string(average_tokens) + "\n");
        terminal.write("  Average generation speed: " + std::to_string(average_tps) + " tok/s\n");
        terminal.write("  Best generation speed: " + std::to_string(best_generation_tps) +
                       " tok/s\n");
        terminal.write("  Worst prompt time: " + std::to_string(worst_prompt_ms) + " ms\n");
        for (size_t i = 0; i < runs.size(); ++i) {
            terminal.write("  Run " + std::to_string(i + 1) + ": " +
                           std::to_string(runs[i].generated_tokens) + " tokens in " +
                           std::to_string(runs[i].generation_ms) + " ms at " +
                           std::to_string(runs[i].tokens_per_second) + " tok/s\n");
        }
    }
    return 0;
}

int run_one_shot(skiffllm::Config& cfg, std::unique_ptr<skiffllm::SkiffEngine>& engine,
                 skiffllm::Session& session, skiffllm::Terminal& terminal,
                 const skiffllm::GenerationOptions& options, const std::string& raw_prompt) {
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

    if (cfg.warmup) {
        warmup_model(*engine, terminal, cfg.json_output);
    }

    std::string attach_error;
    std::string prompt_text = raw_prompt;
    prompt_text = expand_at_paths(prompt_text, attach_error);
    if (!attach_error.empty()) {
        if (cfg.json_output) {
            std::cout << "{\"error\":" << json_escape(attach_error) << "}\n";
        } else {
            terminal.error(attach_error);
        }
        return 1;
    }
    if (!cfg.attach_paths.empty()) {
        const std::string block = make_attach_block(cfg.attach_paths, attach_error);
        if (!attach_error.empty()) {
            if (cfg.json_output) {
                std::cout << "{\"error\":" << json_escape(attach_error) << "}\n";
            } else {
                terminal.error(attach_error);
            }
            return 1;
        }
        prompt_text = block + prompt_text;
    }

    std::vector<skiffllm::ChatMessage> ask = session.conversation();
    ask.push_back({"user", prompt_text});

    skiffllm::GenerationOptions current = options;
    LiveStreamer streamer(terminal);
    if (!cfg.json_output) {
        streamer.reset();
        current.token_callback = [&streamer](const std::string& part) { streamer.write(part); };
        terminal.write("User: " + compact_preview(prompt_text) + "\n", skiffllm::Color::Cyan);
    }

    skiffllm::GenerationResult result;
    std::string error;
    const bool ok = engine->generate(
        ask, current, result, []() { return g_interrupted != 0; }, error);
    streamer.finish();
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

    skiffllm::record_generation(cfg, result.prompt_tokens, result.generated_tokens,
                                result.prompt_ms, result.generation_ms, result.tokens_per_second);

    if (cfg.context_bar && !cfg.json_output) {
        const uint32_t capacity = engine->context_capacity();
        const uint64_t used = static_cast<uint64_t>(result.prompt_tokens) +
                              static_cast<uint64_t>(result.generated_tokens);
        terminal.print_context_bar(used, capacity);
    }

    if (!cfg.output_path.empty()) {
        std::string write_error;
        if (!write_text_file(cfg.output_path, result.text, write_error)) {
            terminal.error(write_error);
        } else if (!cfg.json_output) {
            terminal.success("Answer written to " + cfg.output_path.string());
        }
    }

    session.messages().push_back({"user", prompt_text});
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
    KeepConsoleOpen keep_open(argc == 1 && keep_open_on_no_arguments());
    skiffllm::Config cfg = skiffllm::default_config();
    skiffllm::apply_environment(cfg);

    std::string subcommand;
    std::vector<std::string> sub_args;
    int argument_start = 1;
    if (argc >= 2) {
        const std::string first(argv[1]);
        if (first == "run" || first == "model" || first == "git" || first == "session" ||
            first == "chat-template" || first == "openai" || first == "config" ||
            first == "server") {
            subcommand = first;
            argument_start = 2;
        }
    }
    for (int i = argument_start; i < argc; ++i) {
        sub_args.push_back(argv[i]);
    }

    std::string config_path = cfg.config_path.string();
    for (int i = argument_start; i < argc; ++i) {
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
        cfg.config_path = skiffllm::expand_path(config_path);
    }

    std::error_code ec;
    if (!subcommand.empty()) {
        std::filesystem::create_directories(cfg.model_dir, ec);
    }

    std::string error;
    if (!skiffllm::parse_config_file(cfg.config_path, cfg, error)) {
        std::cerr << error << std::endl;
        return 1;
    }

    std::vector<std::string> arg_storage;
    std::vector<char*> arg_ptrs;
    arg_storage.emplace_back(argv[0]);
    for (const auto& value : sub_args) {
        arg_storage.push_back(value);
    }
    arg_ptrs.reserve(arg_storage.size());
    for (auto& value : arg_storage) {
        arg_ptrs.push_back(value.data());
    }
    if (!skiffllm::parse_args(static_cast<int>(arg_ptrs.size()), arg_ptrs.data(), cfg, error)) {
        std::cerr << error << std::endl;
        return 2;
    }

    if (cfg.debug) {
        cfg.log_llama = true;
    }

    if (cfg.show_help) {
        std::cout << skiffllm::usage(argv[0]);
        return 0;
    }
    if (cfg.show_version) {
        std::cout << "SkiffLLM " << kVersion << " (llama.cpp " << llama_version() << ")\n";
        return 0;
    }
    if (cfg.show_config) {
        skiffllm::print_config(cfg, cfg.json_output);
        return 0;
    }

    if (cfg.backend_info) {
        const skiffllm::SkiffEngine probe(cfg);
        std::cout << "Backends linked into this build: "
                  << (probe.active_backends().empty() ? "CPU-only" : probe.active_backends())
                  << "\n";
        std::cout << "GPU layers configured: " << cfg.n_gpu_layers << "\n";
        std::cout << "Flash attention: " << (cfg.flash_attn ? "yes" : "no") << "\n";
        std::cout << "KV offload: " << (cfg.offload_kqv ? "yes" : "no") << "\n";
        std::cout
            << "For optimal hardware acceleration, rebuild with "
            << "-DSKIFFLLM_LLAMA_BACKEND=cuda (Linux), -DSKIFFLLM_LLAMA_BACKEND=metal (macOS), "
            << "-DSKIFFLLM_LLAMA_BACKEND=vulkan or -DSKIFFLLM_LLAMA_BACKEND=opencl, then pass "
               "--gpu-layers.\n";
        return 0;
    }

    if (subcommand == "model") {
        error.clear();
        const int status = skiffllm::handle_model_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "git") {
        error.clear();
        const int status = skiffllm::handle_git_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "session") {
        error.clear();
        const int status = skiffllm::handle_session_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "config") {
        error.clear();
        const int status = skiffllm::handle_config_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "server") {
        error.clear();
        const int status = skiffllm::handle_server_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "chat-template") {
        error.clear();
        const int status = skiffllm::handle_chat_template_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }
    if (subcommand == "openai") {
        error.clear();
        const int status = skiffllm::handle_openai_command(cfg, sub_args, error);
        if (status >= 0) {
            if (!error.empty()) {
                std::cerr << error << std::endl;
            }
            return status;
        }
    }

    if (!cfg.remember_text.empty()) {
        std::string memory_error;
        if (!skiffllm::append_memory(cfg, cfg.remember_text, memory_error)) {
            std::cerr << memory_error << std::endl;
            return 1;
        }
        std::cout << "Remembered: " << cfg.remember_text << "\n";
        return 0;
    }
    if (!cfg.forget_text.empty()) {
        size_t removed = 0;
        std::string memory_error;
        if (!skiffllm::remove_memory(cfg, cfg.forget_text, removed, memory_error)) {
            std::cerr << memory_error << std::endl;
            return 1;
        }
        std::cout << "Forgot " << removed << " matching memory lines.\n";
        return 0;
    }

    if (!skiffllm::is_stdin_tty()) {
        const std::string piped = skiffllm::read_stdin_all();
        if (!piped.empty()) {
            if (!cfg.one_shot.empty()) {
                cfg.one_shot += "\n\n<context>\n" + piped + "\n</context>\n";
            } else if (cfg.prompt_file.empty() && !cfg.read_stdin) {
                cfg.one_shot = piped;
                cfg.read_stdin = true;
            }
            cfg.interactive = false;
        }
    }

    if (!cfg.summarize_path.empty()) {
        const std::string document = read_file(cfg.summarize_path);
        if (document.empty()) {
            std::cerr << "Summarize file is empty or unreadable: " << cfg.summarize_path.string()
                      << std::endl;
            return 1;
        }
        const std::string instruction =
            cfg.one_shot.empty() ? "Summarize the following document." : cfg.one_shot;
        cfg.one_shot = instruction + "\n\n<document>\n" + document + "\n</document>\n";
        cfg.interactive = false;
    }

    if (!cfg.project_path.empty()) {
        if (!cfg.one_shot.empty()) {
            std::string project_error;
            const std::string block =
                skiffllm::build_project_block(cfg.project_path, project_error);
            if (block.empty()) {
                std::cerr << project_error << std::endl;
                return 1;
            }
            cfg.one_shot = block + "\n" + cfg.one_shot;
        } else if (cfg.interactive) {
            std::cerr << "note: --project applies to one-shot prompts; use a single prompt "
                         "(e.g. `skiffllm --project . \"where is auth implemented?\"`)\n";
        }
    }

    if (cfg.code_mode) {
        if (!cfg.one_shot.empty()) {
            const std::string code_header =
                "You are a careful coding assistant. Propose concrete edits as a "
                "unified diff (file paths, +/-, context). Use the code context below. "
                "Never claim a file was changed; output the proposal only and wait for "
                "a human to apply it.\n\n";
            cfg.one_shot = code_header + cfg.one_shot;
        } else if (cfg.interactive) {
            std::cerr << "note: --code hardens one-shot prompts into a proposal-only, "
                         "no-file-mutation review; it has no effect in the interactive "
                         "shell. Use it with a prompt or via a pipe.\n";
        }
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

    skiffllm::Terminal terminal(cfg);
    std::unique_ptr<skiffllm::SkiffEngine> engine;

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

    if (cfg.reset_history) {
        cfg.save_history = true;
    }

    if (!cfg.export_path.empty()) {
        skiffllm::Session export_session(cfg);
        export_session.set_system_prompt(cfg.system_prompt);
        error.clear();
        if (!export_session.load(error)) {
            terminal.error(error);
            llama_backend_free();
            return 1;
        }
        std::string export_error;
        const std::string markdown = export_markdown(export_session.conversation());
        if (!write_text_file(cfg.export_path, markdown, export_error)) {
            terminal.error(export_error);
            llama_backend_free();
            return 1;
        }
        terminal.success("Conversation exported to " + cfg.export_path.string());
        llama_backend_free();
        return 0;
    }

    if (cfg.model_path.empty() && cfg.model_dir.empty()) {
        std::cerr << "No model configured. Use --model <path.gguf>.\n";
        return 2;
    }

    const std::filesystem::path model_path = cfg.model_path.empty()
                                                 ? std::filesystem::path()
                                                 : skiffllm::expand_path(cfg.model_path.string());
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

    skiffllm::Session session(cfg);
    session.set_system_prompt(cfg.system_prompt);
    error.clear();
    if (!session.load(error)) {
        terminal.error(error);
        llama_backend_free();
        return 1;
    }

    const std::string memories = skiffllm::load_memories(cfg);
    if (!memories.empty()) {
        const std::string current = session.system_prompt();
        session.set_system_prompt(current.empty()
                                      ? "Persistent user facts:\n" + memories
                                      : current + "\n\nPersistent user facts:\n" + memories);
    }

    skiffllm::GenerationOptions options;
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

    if (cfg.serve) {
        std::string model_error;
        if (!choose_model(cfg, engine, model_error)) {
            terminal.error(model_error);
            engine.reset();
            llama_backend_free();
            return 1;
        }
        if (cfg.warmup) {
            warmup_model(*engine, terminal, cfg.json_output);
        }
        if (cfg.server_host == "0.0.0.0" && cfg.api_key.empty()) {
            std::cerr << "WARNING: --serve on 0.0.0.0 has no --api-key set; the\n"
                         "         /v1/* endpoints will be publicly readable by\n"
                         "         anyone who can reach this host. Pass --api-key\n"
                         "         (or set SKIFFLLM_API_KEY) before exposing it\n"
                         "         beyond loopback.\n";
        }
        const int status = skiffllm::run_server(cfg, *engine, terminal, options,
                                                []() { return g_interrupted != 0; });
        engine.reset();
        llama_backend_free();
        return status;
    }

    if (cfg.benchmark_runs > 0) {
        const int status = run_benchmark(cfg, engine, terminal, options);
        engine.reset();
        llama_backend_free();
        return status;
    }

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
