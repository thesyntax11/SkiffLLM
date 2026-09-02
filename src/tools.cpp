#include "skifflm/tools.hpp"
#include "skifflm/engine.hpp"
#include "skifflm/session.hpp"
#include "skifflm/server.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <unistd.h>
#ifndef _POSIX_VERSION
// Minimal fallback for the rare non-POSIX Unix: still try fork/exec.
#endif
#include <sys/wait.h>
#endif

namespace skifflm {
namespace {

// Run an external program without going through a shell. Arguments are passed
// as an argv vector, so user-controlled values (hosts, URLs, model ids, API
// keys) can never be interpreted as shell metacharacters.
bool run_argv(const std::vector<std::string>& argv,
              bool capture_stdout,
              std::string& output,
              std::string& error) {
    output.clear();
    error.clear();
    if (argv.empty()) {
        error = "empty command";
        return false;
    }
    std::vector<const char*> raw;
    raw.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        raw.push_back(arg.c_str());
    }
    raw.push_back(nullptr);

#ifdef _WIN32
    // Windows has no fork; use _spawnvp with an argument vector (never a shell).
    // For captured output we redirect the child's stdout to a temporary file,
    // then read it back. This keeps arbitrary argv values out of any shell.
    std::filesystem::path capture_file;
    int saved_stdout = -1;
    if (capture_stdout) {
        std::error_code ec;
        capture_file = std::filesystem::temp_directory_path(ec) /
                       ("skifflm-capture-" + std::to_string(static_cast<long long>(_getpid())) + ".tmp");
        const int fd = _open(capture_file.string().c_str(),
                             _O_CREAT | _O_WRONLY | _O_TRUNC | _O_BINARY,
                             _S_IREAD | _S_IWRITE);
        if (fd < 0) {
            error = "cannot create capture file";
            return false;
        }
        saved_stdout = _dup(_fileno(stdout));
        if (saved_stdout < 0 || _dup2(fd, _fileno(stdout)) < 0) {
            if (saved_stdout >= 0) {
                _close(saved_stdout);
            }
            _close(fd);
            error = "cannot redirect stdout";
            return false;
        }
        _close(fd);
    }

    const intptr_t spawn_status = _spawnvp(_P_WAIT, argv[0].c_str(), raw.data());

    if (capture_stdout) {
        if (saved_stdout >= 0) {
            _dup2(saved_stdout, _fileno(stdout));
            _close(saved_stdout);
        }
        std::ifstream in(capture_file, std::ios::binary);
        if (in.is_open()) {
            std::ostringstream buffer;
            buffer << in.rdbuf();
            output = buffer.str();
            in.close();
        }
        std::error_code ec;
        std::filesystem::remove(capture_file, ec);
    }

    if (spawn_status == -1) {
        error = "cannot start command: " + argv[0] + " (" + std::strerror(errno) + ")";
        return false;
    }
    if (spawn_status != 0) {
        error = "command exited with status " + std::to_string(spawn_status) + ": " + argv[0];
        return false;
    }
    return true;
#else
    int pipe_fd[2];
    if (capture_stdout && pipe(pipe_fd) != 0) {
        error = "cannot create pipe";
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        if (capture_stdout) {
            close(pipe_fd[0]);
            close(pipe_fd[1]);
        }
        error = "cannot fork";
        return false;
    }
    if (pid == 0) {
        if (capture_stdout) {
            close(pipe_fd[0]);
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[1]);
        }
        execvp(argv[0].c_str(), const_cast<char* const*>(raw.data()));
        // exec failed; report by writing to stderr and exiting non-zero.
        const char msg[] = "failed to execute\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(127);
    }

    int status = 0;
    if (capture_stdout) {
        close(pipe_fd[1]);
        char buffer[4096];
        ssize_t count = 0;
        while ((count = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
            output.append(buffer, static_cast<size_t>(count));
        }
        close(pipe_fd[0]);
    }
    if (waitpid(pid, &status, 0) < 0) {
        error = "waitpid failed";
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (code == 127) {
            error = "executable not found on PATH: " + argv[0] +
                    " (install it, or use the dependency-free client at scripts/api_client.py)";
        } else {
            error = "command exited with status " + std::to_string(code) + ": " + argv[0];
        }
        return false;
    }
    return true;
#endif
}

bool is_hidden_or_build_dir(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name.empty()) {
        return false;
    }
    if (name == ".git" || name == ".hg" || name == ".svn" || name == "build" ||
        name == "build2" || name == "node_modules" || name == "vendor" ||
        name == "_deps" || name == ".gradle" || name == "dist" || name == "out" ||
        name == "target" || name == ".venv" || name == "venv" ||
        name == "__pycache__" || name == ".next" || name == ".cache" ||
        name == ".pytest_cache" || name == ".idea" || name == ".vscode") {
        return true;
    }
    return name.size() > 1 && name[0] == '.';
}

bool is_source(const std::string& ext) {
    static const std::vector<std::string> exts = {
        "cpp", "cc", "cxx", "c", "hpp", "hxx", "hh", "h", "kt", "java",
        "py", "rs", "go", "js", "ts", "tsx", "sh", "cmake"};
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool is_config(const std::string& ext) {
    static const std::vector<std::string> exts = {
        "json", "yaml", "yml", "toml", "ini", "conf", "cfg", "xml",
        "properties", "md", "txt"};
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool looks_like_test(const std::filesystem::path& relative) {
    const std::string lower = [&]() {
        std::string value = relative.string();
        std::transform(value.begin(), value.end(), value.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }();
    return lower.find("test") != std::string::npos ||
           lower.find("spec") != std::string::npos ||
           lower.find("_test") != std::string::npos ||
           lower.find("/tests/") != std::string::npos ||
           lower.find("/test_") != std::string::npos;
}

std::string extension_of(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return ext;
}

struct ProjectFile {
    std::filesystem::path relative;
    uint64_t bytes = 0;
};

bool run_git(const std::vector<std::string>& arguments,
             std::string& output,
             std::string& error) {
    std::vector<std::string> argv;
    argv.reserve(arguments.size() + 1);
    argv.push_back("git");
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    return run_argv(argv, true, output, error);
}










std::string instruction_for_git(const std::string& sub) {
    if (sub == "review") {
        return "You are a senior code reviewer. Review the diff below for bugs, "
               "security issues, error handling problems, and style regressions. "
               "Answer with a short prioritized list: HIGH, MEDIUM or LOW per finding, "
               "include file and line when available, and finish with one line of overall "
               "verdict. Be concrete; do not invent findings.";
    }
    if (sub == "explain") {
        return "Explain the diff below in plain language. Summarize what changed, "
               "why it matters, and call out anything risky. Keep it concise.";
    }
    if (sub == "commit") {
        return "Write a conventional commit message below the diff. The diff is "
               "below. Reply with only the commit subject and body in Git "
               "format, no extra commentary.";
    }
    return "Answer the user's request using the git input below as context.";
}

std::string compact_preview(const std::string& text, std::size_t limit = 160) {
    std::string result;
    result.reserve(std::min(text.size(), limit + 1));
    for (const char ch : text) {
        if (ch == '\n' || ch == '\r' || ch == '\t') {
            result.push_back(' ');
        } else {
            result.push_back(ch);
        }
        if (result.size() >= limit) {
            result += "...";
            break;
        }
    }
    return result;
}

std::filesystem::path session_dir_for(const Config& cfg) {
    if (!cfg.history_path.empty() && !cfg.history_path.parent_path().empty()) {
        return cfg.history_path.parent_path();
    }
    return std::filesystem::path(".");
}

}  // namespace

bool load_usage_stats(const Config& cfg, UsageStats& stats, std::string& error) {
    std::ifstream input(metrics_path_for(cfg));
    if (!input.is_open()) {
        return true;
    }
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string field;
        std::vector<std::string> parts;
        while (std::getline(row, field, '\t')) {
            parts.push_back(field);
        }
        if (parts.size() < 6) {
            continue;
        }
        const int prompt = std::atoi(parts[1].c_str());
        const int generated = std::atoi(parts[2].c_str());
        const double prompt_ms = std::atof(parts[3].c_str());
        const double generation_ms = std::atof(parts[4].c_str());
        if (prompt < 0 || generated < 0 || prompt_ms < 0 || generation_ms < 0) {
            continue;
        }
        stats.sessions += 1;
        stats.messages += static_cast<uint64_t>(prompt + generated);
        stats.prompt_tokens += static_cast<uint64_t>(prompt);
        stats.generated_tokens += static_cast<uint64_t>(generated);
        stats.total_prompt_ms += prompt_ms;
        stats.total_generation_ms += generation_ms;
    }
    if (!input.bad()) {
        return true;
    }
    error = "failed to read metrics file: " + metrics_path_for(cfg).string();
    return false;
}

bool write_config_file(const std::filesystem::path& path,
                       const Config& cfg,
                       std::string& error) {
    if (path.empty()) {
        error = "config path is empty";
        return false;
    }
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create config directory: " + parent.string();
            return false;
        }
    }
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        error = "cannot open config file for writing: " + path.string();
        return false;
    }
    out << "# SkiffLLM configuration\n";
    out << "# One key=value (option) per line. CLI flags override this file.\n\n";
    out << "model=" << (cfg.model_path.empty() ? "" : cfg.model_path.string()) << "\n";
    out << "model-dir=" << cfg.model_dir.string() << "\n";
    out << "history=" << cfg.history_path.string() << "\n";
    out << "session=" << cfg.session_name << "\n";
    out << "system=" << cfg.system_prompt << "\n";
    out << "chat-template=" << cfg.chat_template << "\n";
    out << "profile=" << cfg.profile_name << "\n";
    out << "ctx=" << cfg.context_size << "\n";
    out << "batch=" << cfg.batch_size << "\n";
    if (cfg.n_ubatch > 0) {
        out << "ubatch=" << cfg.n_ubatch << "\n";
    }
    if (cfg.reserve_ctx > 0) {
        out << "reserve-ctx=" << cfg.reserve_ctx << "\n";
    }
    if (cfg.n_threads > 0) {
        out << "threads=" << cfg.n_threads << "\n";
    }
    out << "gpu-layers=" << cfg.n_gpu_layers << "\n";
    out << "temp=" << cfg.temperature << "\n";
    out << "top-p=" << cfg.top_p << "\n";
    out << "top-k=" << cfg.top_k << "\n";
    out << "min-p=" << cfg.min_p << "\n";
    out << "typical=" << cfg.typical_p << "\n";
    out << "repeat-penalty=" << cfg.repeat_penalty << "\n";
    out << "repeat-last-n=" << cfg.repeat_last_n << "\n";
    out << "n-predict=" << cfg.n_predict << "\n";
    out << "numa=" << (cfg.numa ? "yes" : "no") << "\n";
    out << "flash-attn=" << (cfg.flash_attn ? "yes" : "no") << "\n";
    out << "mlock=" << (cfg.use_mlock ? "yes" : "no") << "\n";
    out << "mmap=" << (cfg.use_mmap ? "yes" : "no") << "\n";
    out << "context-bar=" << (cfg.context_bar ? "yes" : "no") << "\n";
    out << "backend-info=" << (cfg.backend_info ? "yes" : "no") << "\n";
    out << "serve=" << (cfg.serve ? "yes" : "no") << "\n";
    out << "host=" << cfg.server_host << "\n";
    out << "port=" << cfg.server_port << "\n";
    if (!cfg.api_key.empty()) {
        out << "api-key=" << cfg.api_key << "\n";
    }
    for (const auto& stop : cfg.stop_sequences) {
        out << "stop=" << stop << "\n";
    }
    out.flush();
    if (!out) {
        error = "failed to write config file";
        return false;
    }
    return true;
}

int handle_config_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error) {
    const std::string action = args.empty() ? "path" : args[0];
    if (action == "help") {
        std::cout << "Usage: skifflm config path|show|init\n";
        return 0;
    }
    if (action == "path") {
        std::cout << cfg.config_path.string() << "\n";
        return 0;
    }
    if (action == "show") {
        print_config(cfg, cfg.json_output);
        return 0;
    }
    if (action == "init" || action == "write") {
        std::string write_error;
        if (!write_config_file(cfg.config_path, cfg, write_error)) {
            error = write_error;
            return 1;
        }
        std::cout << "Config written to " << cfg.config_path.string() << "\n";
        return 0;
    }
    error = "unknown config action: " + action + " (use path, show or init)";
    return 2;
}

int handle_server_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error) {
    if (args.empty() || args[0] == "help") {
        std::cout << "Usage: skifflm server health [--json] [--host <addr>] [--port <n>]\n";
        std::cout << "       skifflm server help\n";
        return 0;
    }
    if (args[0] == "health") {
        std::string host = cfg.server_host;
        int port = cfg.server_port;
        bool as_json = false;
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--json") {
                as_json = true;
            } else if (args[i] == "--host" && i + 1 < args.size()) {
                host = args[++i];
            } else if (args[i] == "--port" && i + 1 < args.size()) {
                port = std::atoi(args[++i].c_str());
            }
        }
        std::string output;
        std::vector<std::string> health_argv = {
            "curl", "-sf", "--max-time", "3",
            "http://" + host + ":" + std::to_string(port) + "/health"};
        if (!run_argv(health_argv, true, output, error)) {
            if (as_json) {
                std::cout << "{\"reachable\":false,\"error\":\"server is not reachable at http://"
                          << host << ":" << port << "\"}\n";
            } else {
                std::cout << "Server not responding at http://" << host << ":" << port << "\n";
            }
            error.clear();
            return 1;
        }
        if (as_json) {
            std::cout << output;
        } else {
            std::cout << "Server healthy at http://" << host << ":" << port << "\n";
            std::cout << output;
        }
        return 0;
    }
    error = "unknown server action: " + args[0] + " (use health or help)";
    return 2;
}

int handle_chat_template_command(Config& cfg,
                                 const std::vector<std::string>& args,
                                 std::string& error) {
    const std::string action = args.empty() ? "list" : args[0];
    if (action == "list" || action == "ls" || action == "help") {
        std::cout << "Known chat templates:\n";
        std::cout << "  chatml      <|im_start|> ... <|im_end|> (Qwen, many models)\n";
        std::cout << "  llama3      <|start_header_id|> ... (Llama 3.x)\n";
        std::cout << "  mistral     [INST] ... [/INST] (Mistral/Mixtral)\n";
        std::cout << "  gemma       <start_of_turn> ... (Gemma/Gemma 2)\n";
        std::cout << "  phi3        <|user|> ... <|end|> (Phi-3)\n";
        std::cout << "  vicuna      USER: ... ASSISTANT: (older chat models)\n";
        std::cout << "  default     generic assistant/chat style\n";
        std::cout << "\nUse `skifflm chat-template detect --model <file.gguf>` to read the\n";
        std::cout << "template embedded in a specific model, or run `skifflm model info <id>`.\n";
        return 0;
    }
    if (action == "detect" || action == "info") {
        std::string model_path;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--model" && i + 1 < args.size()) {
                model_path = args[++i];
            } else if (args[i].compare(0, 8, "--model=") == 0) {
                model_path = args[i].substr(8);
            }
        }
        if (model_path.empty()) {
            model_path = cfg.model_path.string();
        }
        if (model_path.empty()) {
            error = "usage: skifflm chat-template detect --model <path.gguf> (or set --model)";
            return 2;
        }
        cfg.model_path = expand_path(model_path);
        LlmEngine engine(cfg);
        std::string load_error;
        if (!engine.load(load_error)) {
            error = "cannot load model to detect template: " + load_error;
            return 1;
        }
        const std::string detected = engine.info().chat_template.empty()
                                         ? "(none / model default)"
                                         : engine.info().chat_template;
        std::cout << "Model: " << cfg.model_path.string() << "\n";
        std::cout << "Detected chat template: " << detected << "\n";
        std::cout << "  overridden internally as: "
                  << (engine.info().chat_template.empty() ? "chatml" : engine.info().chat_template)
                  << "\n";
        if (detected != "(none / model default)") {
            std::cout << "Use with: --chat-template \"" << detected << "\"\n";
        }
        return 0;
    }
    error = "unknown chat-template action: " + action + " (use list, detect or info)";
    return 2;
}

std::string json_openai_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (const char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(ch));
                    out += buffer;
                } else {
                    out.push_back(ch);
                }
        }
    }
    return out;
}

std::string json_openai_unescape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }
        const char next = value[++i];
        switch (next) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'u': {
                if (i + 4 < value.size()) {
                    unsigned int code = 0;
                    for (int j = 1; j <= 4; ++j) {
                        const char c = value[i + j];
                        code <<= 4;
                        if (c >= '0' && c <= '9') code |= static_cast<unsigned int>(c - '0');
                        else if (c >= 'a' && c <= 'f') code |= static_cast<unsigned int>(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') code |= static_cast<unsigned int>(c - 'A' + 10);
                    }
                    i += 4;
                    if (code < 0x80) {
                        out.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                }
                break;
            }
            default: out.push_back(next); break;
        }
    }
    return out;
}

bool extract_openai_content(const std::string& json, std::string& content) {
    std::size_t pos = json.find("\"content\"");
    if (pos == std::string::npos) {
        return false;
    }
    pos += std::strlen("\"content\"");
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != ':') {
        return false;
    }
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos >= json.size() || json[pos] != '"') {
        return false;
    }
    std::string raw;
    ++pos;
    while (pos < json.size()) {
        const char ch = json[pos];
        if (ch == '\\' && pos + 1 < json.size()) {
            raw.push_back(ch);
            raw.push_back(json[pos + 1]);
            pos += 2;
            continue;
        }
        if (ch == '"') {
            break;
        }
        raw.push_back(ch);
        ++pos;
    }
    content = json_openai_unescape(raw);
    return true;
}

int handle_openai_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error) {
    (void)cfg;
    std::string base_url = "http://127.0.0.1:8080";
    std::string model = "skifflm-local";
    std::string api_key;
    std::string prompt;
    bool stream = false;
    bool json_output = true;
    float temperature = 0.7f;
    int max_tokens = 0;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string arg = args[i];
        if (arg == "--base-url" || arg == "--base") {
            base_url = args[++i];
        } else if (arg.compare(0, 8, "--base=") == 0) {
            base_url = arg.substr(8);
        } else if (arg == "--model") {
            model = args[++i];
        } else if (arg.compare(0, 8, "--model=") == 0) {
            model = arg.substr(8);
        } else if (arg == "--api-key" || arg == "--key") {
            api_key = args[++i];
        } else if (arg.compare(0, 10, "--api-key=") == 0) {
            api_key = arg.substr(10);
        } else if (arg == "--prompt") {
            prompt = args[++i];
        } else if (arg.compare(0, 9, "--prompt=") == 0) {
            prompt = arg.substr(9);
        } else if (arg == "--stream") {
            stream = true;
        } else if (arg == "--no-json") {
            json_output = false;
        } else if (arg == "--json") {
            json_output = true;
        } else if (arg == "--temp" || arg == "--temperature") {
            temperature = std::atof(args[++i].c_str());
        } else if (arg.compare(0, 6, "--temp=") == 0) {
            temperature = std::atof(arg.substr(6).c_str());
        } else if (arg == "--max-tokens" || arg == "--n-predict") {
            max_tokens = std::atoi(args[++i].c_str());
        } else if (arg.compare(0, 13, "--max-tokens=") == 0) {
            max_tokens = std::atoi(arg.substr(13).c_str());
        } else if (!prompt.empty()) {
            prompt += " " + arg;
        } else {
            prompt = arg;
        }
    }
    if (prompt.empty()) {
        error = "usage: skifflm openai [--base-url http://127.0.0.1:8080] [--model id] "
                "[--stream] [--json] [--temp <t>] [--max-tokens <n>] \"prompt\"";
        return 2;
    }

    std::ostringstream payload;
    payload << "{\"model\":\"" << json_openai_escape(model)
            << "\",\"messages\":[{\"role\":\"user\",\"content\":\"" << json_openai_escape(prompt)
            << "\"}],\"stream\":" << (stream ? "true" : "false")
            << ",\"temperature\":" << temperature;
    if (max_tokens > 0) {
        payload << ",\"max_tokens\":" << max_tokens;
    }
    payload << "}\n";

#ifdef _WIN32
    const long long pid_value = static_cast<long long>(_getpid());
#else
    const long long pid_value = static_cast<long long>(getpid());
#endif
    const std::filesystem::path payload_path =
        std::filesystem::temp_directory_path() /
        ("skifflm-openai-" + std::to_string(pid_value) + ".json");
    {
        std::ofstream out(payload_path, std::ios::trunc | std::ios::binary);
        if (!out.is_open()) {
            error = "cannot write payload file: " + payload_path.string();
            return 1;
        }
        out << payload.str();
        if (!out) {
            error = "failed to write payload file";
            return 1;
        }
    }
    // Remove the request payload as soon as curl has consumed it.
    struct PayloadCleanup {
        std::filesystem::path path;
        ~PayloadCleanup() {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
    } payload_cleanup{payload_path};

    const std::string endpoint = base_url + "/v1/chat/completions";
    std::vector<std::string> curl_argv = {"curl", "-sS"};
    if (stream) {
        curl_argv.push_back("-N");
    }
    curl_argv.push_back("-X");
    curl_argv.push_back("POST");
    curl_argv.push_back(endpoint);
    curl_argv.push_back("-H");
    curl_argv.push_back("Content-Type: application/json");
    curl_argv.push_back("-H");
    curl_argv.push_back("Accept: text/event-stream");
    if (!api_key.empty()) {
        curl_argv.push_back("-H");
        curl_argv.push_back("Authorization: Bearer " + api_key);
    }
    curl_argv.push_back("--data-binary");
    curl_argv.push_back("@" + payload_path.string());

    if (stream) {
        // Let curl write directly to the terminal; capture nothing.
        std::string ignore;
        if (!run_argv(curl_argv, false, ignore, error)) {
            error = "request to " + endpoint + " failed: " + error;
            return 1;
        }
        return 0;
    }

    std::string response;
    if (!run_argv(curl_argv, true, response, error)) {
        error = "request to " + endpoint + " failed: " + error;
        return 1;
    }
    std::string content;
    const bool found = extract_openai_content(response, content);
    if (json_output || !found) {
        std::cout << response;
        if (!response.empty() && response.back() != '\n') {
            std::cout << "\n";
        }
    } else {
        std::cout << content << "\n";
    }
    return 0;
}

std::filesystem::path metrics_path_for(const Config& cfg) {
    if (!cfg.history_path.empty() && !cfg.history_path.parent_path().empty()) {
        return cfg.history_path.parent_path() / "metrics.txt";
    }
    return std::filesystem::path("metrics.txt");
}



void print_usage_stats(const Config& cfg, bool as_json) {
    UsageStats stats;
    std::string error;
    if (!load_usage_stats(cfg, stats, error)) {
        std::cerr << error << std::endl;
        return;
    }
    const double total_ms = stats.total_prompt_ms + stats.total_generation_ms;
    const double tps = total_ms > 0.0
                           ? static_cast<double>(stats.generated_tokens) / (total_ms / 1000.0)
                           : 0.0;
    if (as_json) {
        std::cout << "{\"sessions\":" << stats.sessions
                  << ",\"messages\":" << stats.messages
                  << ",\"prompt_tokens\":" << stats.prompt_tokens
                  << ",\"generated_tokens\":" << stats.generated_tokens
                  << ",\"total_time_ms\":" << total_ms
                  << ",\"avg_tokens_per_second\":" << tps << "}\n";
        return;
    }
    std::cout << "SkiffLLM usage stats\n";
    std::cout << "  Generations:      " << stats.sessions << "\n";
    std::cout << "  Messages:         " << stats.messages << "\n";
    std::cout << "  Prompt tokens:    " << stats.prompt_tokens << "\n";
    std::cout << "  Generated tokens: " << stats.generated_tokens << "\n";
    std::cout << "  Total time:       " << (total_ms / 1000.0) << " s\n";
    std::cout << "  Avg speed:        " << tps << " tok/s\n";
    std::cout << "  Metrics file:     " << metrics_path_for(cfg).string() << "\n";
}

void record_generation(const Config& cfg,
                       int prompt_tokens,
                       int generated_tokens,
                       double prompt_ms,
                       double generation_ms,
                       double tokens_per_second) {
    const std::filesystem::path path = metrics_path_for(cfg);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out.is_open()) {
        return;
    }
    // One tab-separated line per generation. Kept plain-text so users can
    // inspect and truncate it easily.
    out << std::time(nullptr) << "\t" << prompt_tokens << "\t" << generated_tokens
        << "\t" << prompt_ms << "\t" << generation_ms << "\t" << tokens_per_second
        << "\n";
    out.close();
}


const std::vector<CatalogModel>& model_catalog() {
    static const std::vector<CatalogModel> catalog = {
        {"qwen2.5-0.5b", "Qwen2.5 0.5B Instruct",
         "Best first model on older machines; fast and small.",
         "Qwen/Qwen2.5-0.5B-Instruct-GGUF", "qwen2.5-0.5b-instruct-q4_k_m.gguf",
         "~1 GB working set", "Apache-2.0", 491400032, true},
        {"qwen3-0.6b", "Qwen3 0.6B Instruct",
         "Small Qwen3 with optional thinking mode; good quality per byte.",
         "bartowski/Qwen_Qwen3-0.6B-GGUF", "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
         "~1 GB working set", "Apache-2.0", 484220320, true},
        {"llama3.2-1b", "Llama 3.2 1B Instruct",
         "Balanced quality and speed for typical CPUs.",
         "bartowski/Llama-3.2-1B-Instruct-GGUF", "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
         "~1.5 GB working set", "Llama 3.2 Community License", 807694464, true},
        {"smollm2-1.7b", "SmolLM2 1.7B Instruct",
         "Mid-size option with a good quality-to-speed ratio.",
         "bartowski/SmolLM2-1.7B-Instruct-GGUF", "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
         "~2 GB working set", "Apache-2.0", 1055609824, false},
        {"phi3.5-mini", "Phi-3.5 Mini Instruct",
         "Better reasoning; needs roughly 4 GB working set.",
         "bartowski/Phi-3.5-mini-instruct-GGUF", "Phi-3.5-mini-instruct-Q4_K_M.gguf",
         "~4 GB working set", "MIT", 2393232672, false},
    };
    return catalog;
}

const CatalogModel* find_catalog_model(const std::string& id) {
    for (const auto& model : model_catalog()) {
        if (model.id == id) {
            return &model;
        }
    }
    return nullptr;
}

std::string build_project_block(const std::filesystem::path& root, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        error = "project directory does not exist: " + root.string();
        return {};
    }

    std::vector<ProjectFile> files;
    uint64_t total_source = 0;
    uint64_t total_tests = 0;
    uint64_t total_config = 0;

    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) {
            break;
        }
        const auto path = it->path();
        const auto relative = std::filesystem::relative(path, root, ec);
        if (ec) {
            continue;
        }
        if (relative.empty()) {
            continue;
        }
        for (const auto& part : relative) {
            if (is_hidden_or_build_dir(part)) {
                it.disable_recursion_pending();
                break;
            }
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::string ext = extension_of(path);
        const uint64_t size = static_cast<uint64_t>(it->file_size(ec));
        if (ec) {
            continue;
        }
        files.push_back({relative, size});
        if (is_source(ext)) {
            total_source += 1;
        }
        if (is_config(ext)) {
            total_config += 1;
        }
        if (looks_like_test(relative)) {
            total_tests += 1;
        }
    }

    std::sort(files.begin(), files.end(), [](const ProjectFile& a, const ProjectFile& b) {
        return a.relative < b.relative;
    });

    std::ostringstream out;
    out << "<project root=\"" << root.string() << "\">\n";
    out << "<summary>\n";
    out << "files: " << files.size() << "\n";
    out << "source files: " << total_source << "\n";
    out << "test files: " << total_tests << "\n";
    out << "config files: " << total_config << "\n";
    out << "</summary>\n";
    out << "<file-index>\n";

    const size_t max_index = 600;
    for (size_t i = 0; i < files.size() && i < max_index; ++i) {
        out << files[i].relative.string() << " (" << files[i].bytes << " bytes)\n";
    }
    if (files.size() > max_index) {
        out << "... " << (files.size() - max_index) << " more files\n";
    }
    out << "</file-index>\n";

    // Include a bounded slice of the most useful source and config files.
    const size_t max_included = 20;
    const size_t per_file_limit = 6000;
    const size_t total_limit = 60000;
    size_t included = 0;
    size_t total = 0;
    for (const auto& file : files) {
        if (included >= max_included || total >= total_limit) {
            break;
        }
        const std::string ext = extension_of(file.relative);
        if (!is_source(ext) && !is_config(ext)) {
            continue;
        }
        std::ifstream input(root / file.relative, std::ios::binary);
        if (!input.is_open()) {
            continue;
        }
        std::string content;
        content.reserve(std::min<uint64_t>(file.bytes, per_file_limit));
        char buffer[4096];
        while (content.size() < per_file_limit && input.read(buffer, sizeof(buffer))) {
            content.append(buffer, static_cast<size_t>(input.gcount()));
        }
        input.close();
        out << "<file path=\"" << file.relative.string() << "\">\n";
        out << content.substr(0, per_file_limit);
        out << "\n</file>\n";
        included += 1;
        total += content.size();
    }
    out << "</project>\n";
    return out.str();
}

bool is_gguf_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }
    char header[4] = {};
    input.read(header, 4);
    return input.gcount() == 4 && std::memcmp(header, "GGUF", 4) == 0;
}

bool hash_file_sha256(const std::filesystem::path& path,
                      std::string& hash,
                      std::string& error) {
    std::string output;
    std::vector<std::string> argv;
#ifdef _WIN32
    argv = {"certutil", "-hashfile", path.string(), "SHA256"};
#else
    argv = {"sha256sum", path.string()};
#endif
    if (!run_argv(argv, true, output, error)) {
        return false;
    }
    hash.clear();
    for (const unsigned char ch : output) {
        if (std::isxdigit(ch)) {
            hash.push_back(static_cast<char>(ch));
        } else if (!hash.empty()) {
            break;
        }
    }
    if (hash.size() < 64) {
        error = "could not parse SHA-256 from checksum output";
        return false;
    }
    hash.resize(64);
    for (char& ch : hash) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return true;
}

bool hash_matches_sidecar(const std::filesystem::path& model_path,
                          std::string& error) {
    const std::filesystem::path sidecar = model_path.string() + ".sha256";
    std::ifstream input(sidecar);
    if (!input.is_open()) {
        error = "no checksum sidecar found at " + sidecar.string() +
                "; run `skifflm model verify --update` to store the local hash";
        return false;
    }
    std::string expected;
    std::getline(input, expected);
    for (char& ch : expected) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    const std::string needle = model_path.filename().string();
    if (const std::size_t pos = expected.find(needle); pos != std::string::npos) {
        expected = expected.substr(0, pos);
    }
    std::string hex;
    hex.reserve(expected.size());
    for (const char ch : expected) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            hex.push_back(ch);
        }
    }
    expected = hex;
    if (expected.size() != 64) {
        error = "malformed checksum sidecar at " + sidecar.string();
        return false;
    }
    std::string actual;
    if (!hash_file_sha256(model_path, actual, error)) {
        return false;
    }
    if (actual != expected) {
        error = "SHA-256 mismatch: model file is corrupt or was modified\n"
                "  expected " + expected + "\n"
                "  actual   " + actual;
        return false;
    }
    return true;
}

bool write_checksum_sidecar(const std::filesystem::path& model_path,
                            std::string& error) {
    std::string hash;
    if (!hash_file_sha256(model_path, hash, error)) {
        return false;
    }
    const std::filesystem::path sidecar = model_path.string() + ".sha256";
    std::ofstream output(sidecar, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        error = "cannot write checksum sidecar: " + sidecar.string();
        return false;
    }
    output << hash << "  " << model_path.filename().string() << "\n";
    if (!output) {
        error = "failed to write checksum sidecar";
        return false;
    }
    return true;
}

bool verify_model_file(const std::filesystem::path& path,
                       uint64_t expected_bytes,
                       bool update_checksum,
                       std::string& detail) {
    std::error_code ec;
    detail.clear();
    if (!std::filesystem::exists(path, ec)) {
        detail = "model file does not exist: " + path.string();
        return false;
    }
    if (!is_gguf_file(path)) {
        detail = "not a valid GGUF model (bad magic header): " + path.string();
        return false;
    }
    const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(path, ec));
    if (ec) {
        detail = "cannot read model file size: " + ec.message();
        return false;
    }
    if (expected_bytes > 0 && size != expected_bytes) {
        // The catalog records a snapshot at publish time. A model maintainer
        // can legitimately re-upload a revision with a different size, so this
        // is an advisory note rather than a corruption failure. The GGUF header
        // and (when present) the SHA-256 sidecar remain the authority.
        detail += "size differs from the catalog snapshot (expected " +
                  std::to_string(expected_bytes) + " bytes, found " +
                  std::to_string(size) + " bytes); ";
    }
    std::string checksum_error;
    const std::filesystem::path sidecar = path.string() + ".sha256";
    const bool sidecar_exists = std::filesystem::exists(sidecar, ec);
    if (update_checksum) {
        if (!write_checksum_sidecar(path, checksum_error)) {
            detail = checksum_error;
            return false;
        }
        detail += "checksum sidecar written; ";
    } else if (!hash_matches_sidecar(path, checksum_error)) {
        if (sidecar_exists) {
            // A present sidecar is authoritative: a mismatch means the model is
            // corrupt or modified, and must not be silently accepted.
            detail = checksum_error;
            return false;
        }
        // A missing sidecar is informational, not a corruption signal.
        detail += checksum_error + "; ";
    }
    return true;
}

int handle_model_command(Config& cfg,
                         const std::vector<std::string>& args,
                         std::string& error) {
    const std::string action = args.empty() ? "list" : args[0];
    std::error_code ec;
    std::filesystem::create_directories(cfg.model_dir, ec);

    auto installed = [&](const CatalogModel& model) -> bool {
        if (cfg.model_dir.empty()) {
            return false;
        }
        return std::filesystem::exists(cfg.model_dir / model.file, ec);
    };

    if (action == "list" || action == "ls") {
        std::cout << std::left;
        std::cout << std::setw(16) << "ID"
                  << std::setw(24) << "NAME"
                  << std::setw(12) << "SIZE"
                  << std::setw(24) << "RAM"
                  << std::setw(30) << "LICENSE"
                  << "INSTALLED\n";
        std::cout << std::string(106, '-') << "\n";
        for (const auto& model : model_catalog()) {
            std::cout << std::setw(16) << model.id
                      << std::setw(24) << model.name.substr(0, 23)
                      << std::setw(12) << to_human_bytes(model.bytes)
                      << std::setw(24) << model.ram_note
                      << std::setw(30) << model.license.substr(0, 29)
                      << (installed(model) ? "yes" : "no") << "\n";
        }
        std::cout << "\nInstall with: skifflm model install <id>\n";
        return 0;
    }

    if (action == "info" || action == "show") {
        if (args.size() < 2) {
            error = "usage: skifflm model info <id>";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        std::cout << "ID:          " << model->id << "\n";
        std::cout << "Name:        " << model->name << "\n";
        std::cout << "Description: " << model->description << "\n";
        std::cout << "Repo:       " << model->repo << "\n";
        std::cout << "File:       " << model->file << "\n";
        std::cout << "Size:       " << to_human_bytes(model->bytes) << "\n";
        std::cout << "RAM:        " << model->ram_note << "\n";
        std::cout << "License:    " << model->license << "\n";
        std::cout << "Installed:  " << (installed(*model) ? "yes" : "no") << "\n";
        const std::filesystem::path target = cfg.model_dir / model->file;
        if (installed(*model)) {
            std::string detail;
            const bool ok = verify_model_file(target, model->bytes, false, detail);
            std::cout << "Integrity:  " << (ok ? "ok" : "needs attention") << "\n";
            const bool has_sidecar = std::filesystem::exists(target.string() + ".sha256", ec);
            std::cout << "Checksum:   " << (has_sidecar ? "recorded" : "not recorded")
                      << " (use `skifflm model verify " << model->id << " --update`)\n";
        }
        return 0;
    }

    if (action == "install" || action == "get") {
        if (args.size() < 2) {
            error = "usage: skifflm model install <id>";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        if (installed(*model)) {
            std::cout << model->file << " is already installed at "
                      << (cfg.model_dir / model->file).string() << "\n";
            return 0;
        }

        // Find the fetch helper next to the installed binary or in the source
        // checkout.
        const std::filesystem::path project_root =
            std::filesystem::current_path();
        std::vector<std::filesystem::path> candidates = {
            project_root / "scripts" / "model_fetch.py",
        };
        bool found = false;
        std::filesystem::path helper;
        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate, ec) &&
                std::filesystem::is_regular_file(candidate, ec)) {
                helper = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            error =
                "model_fetch.py not found. Install a model with:\n"
                "  python3 scripts/model_fetch.py --model " + model->id +
                " --output-dir " + cfg.model_dir.string();
            return 1;
        }

        std::string unused_out;
        std::string download_err;
        const bool download_ok = run_argv(
            {"python3", helper.string(), "--model", model->id,
             "--output-dir", cfg.model_dir.string()},
            false, unused_out, download_err);
        if (!download_ok) {
            error = "model download failed for " + model->id +
                    (download_err.empty() ? "" : ": " + download_err);
            return 1;
        }
        std::cout << model->file << " installed in " << cfg.model_dir.string() << "\n";
        // Validate the freshly downloaded file before accepting it as usable.
        std::string verify_detail;
        if (!verify_model_file(cfg.model_dir / model->file, model->bytes, false,
                               verify_detail)) {
            std::cout << "Warning: " << verify_detail << "\n";
        }
        return 0;
    }

    if (action == "verify" || action == "check") {
        if (args.size() < 2) {
            error = "usage: skifflm model verify <id> [--update]";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        const std::filesystem::path target = cfg.model_dir / model->file;
        if (!std::filesystem::exists(target, ec)) {
            std::cout << model->file << " is not installed. Run `skifflm model install "
                      << model->id << "` first.\n";
            return 1;
        }
        const bool update = std::find(args.begin(), args.end(), "--update") != args.end();
        std::string detail;
        const bool ok = verify_model_file(target, model->bytes, update, detail);
        if (ok) {
            std::error_code size_ec;
            const uint64_t actual =
                static_cast<uint64_t>(std::filesystem::file_size(target, size_ec));
            std::cout << "Model verified: " << target.string() << "\n";
            std::cout << "  GGUF header: ok\n";
            std::cout << "  Size: " << to_human_bytes(actual) << " ("
                      << actual << " bytes)\n";
            const bool has_sidecar =
                std::filesystem::exists(target.string() + ".sha256", ec);
            std::cout << "  SHA-256: " << (has_sidecar ? "match" : "not recorded")
                      << (update ? " (written)" : "") << "\n";
            if (!detail.empty()) {
                std::cout << "  note: " << detail << "\n";
            }
            return 0;
        }
        std::cout << "Model verification failed: " << target.string() << "\n";
        std::cout << "  " << detail << "\n";
        return 1;
    }

    if (action == "remove" || action == "rm") {
        if (args.size() < 2) {
            error = "usage: skifflm model remove <id> [--force]";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        const std::filesystem::path target = cfg.model_dir / model->file;
        if (!std::filesystem::exists(target, ec)) {
            std::cout << model->file << " is not installed.\n";
            return 0;
        }
        const bool force = std::find(args.begin(), args.end(), "--force") != args.end();
        if (!force) {
            std::cout << target.string() << "\n"
                      << "Re-run with --force to delete this file.\n";
            return 0;
        }
        if (!std::filesystem::remove(target, ec) || ec) {
            error = "could not remove " + target.string() + ": " + ec.message();
            return 1;
        }
        std::cout << "Removed " << model->file << "\n";
        return 0;
    }

    error = "unknown model action: " + action +
            " (use list, info, install or remove)";
    return 2;
}

int handle_git_command(Config& cfg,
                       const std::vector<std::string>& args,
                       std::string& error) {
    const std::string action = args.empty() ? "diff" : args[0];
    bool use_cached = false;
    std::vector<std::string> git_args;

    if (action == "diff" || action == "review" || action == "explain" ||
        action == "commit") {
        use_cached = std::find(args.begin(), args.end(), "--cached") != args.end();
        git_args = use_cached ? std::vector<std::string>{"diff", "--cached"}
                              : std::vector<std::string>{"diff"};
        if (action == "commit" && !use_cached) {
            std::cout << "No --cached diff provided. Use `skifflm git commit --cached` "
                      << "after staging changes.\n";
            return 1;
        }
        std::string diff;
        if (!run_git(git_args, diff, error)) {
            return 1;
        }
        if (diff.empty()) {
            std::cout << "No diff to analyze.\n";
            return 0;
        }
        std::string instruction = instruction_for_git(action);
        cfg.one_shot = instruction + "\n\n<git input>\n" + diff + "\n</git input>\n";
        cfg.interactive = false;
        return -1;  // continue to generation
    }

    if (action == "log") {
        std::string log;
        if (!run_git({"log", "-n", "40", "--oneline"}, log, error)) {
            return 1;
        }
        cfg.one_shot =
            "Summarize this git history into a short release-note style outline.\n\n"
            "<git log>\n" + log + "\n</git log>\n";
        cfg.interactive = false;
        return -1;
    }

    if (action == "status") {
        std::string status;
        if (!run_git({"status", "--short"}, status, error)) {
            return 1;
        }
        std::cout << status;
        return 0;
    }

    error = "unknown git action: " + action +
            " (use diff, review, explain, commit, log or status)";
    return 2;
}

std::filesystem::path session_file_for(const Config& cfg, const std::string& name) {
    const bool has_separator = name.find('/') != std::string::npos ||
                               name.find('\\') != std::string::npos;
    const bool has_skif = name.size() > 5 && name.compare(name.size() - 5, 5, ".skif") == 0;
    if (has_separator || has_skif) {
        return expand_path(name);
    }
    std::string sanitized;
    sanitized.reserve(name.size());
    for (const unsigned char ch : name) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.') {
            sanitized.push_back(static_cast<char>(ch));
        } else {
            sanitized.push_back('_');
        }
    }
    if (sanitized.empty()) {
        sanitized = "default";
    }
    return session_dir_for(cfg) / (sanitized + ".skif");
}

std::vector<std::filesystem::path> list_session_files(const Config& cfg,
                                                      std::string& error) {
    std::vector<std::filesystem::path> result;
    const std::filesystem::path dir = session_dir_for(cfg);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return result;
    }
    if (!std::filesystem::is_directory(dir, ec)) {
        error = "history directory is not a directory: " + dir.string();
        return {};
    }
    for (auto it = std::filesystem::directory_iterator(dir, ec);
         it != std::filesystem::directory_iterator();
         it.increment(ec)) {
        if (ec) {
            break;
        }
        const std::filesystem::path path = it->path();
        if (it->is_regular_file(ec) && path.extension() == ".skif") {
            result.push_back(path);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string load_memories(const Config& cfg) {
    if (cfg.memory_path.empty()) {
        return {};
    }
    std::ifstream input(cfg.memory_path);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream out;
    std::string line;
    bool first = true;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        if (!first) {
            out << "\n";
        }
        first = false;
        out << trimmed;
    }
    return out.str();
}

bool append_memory(const Config& cfg, const std::string& text, std::string& error) {
    const std::string value = trim(text);
    if (value.empty()) {
        error = "memory text is empty";
        return false;
    }
    std::error_code ec;
    const auto parent = cfg.memory_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create memory directory: " + parent.string();
            return false;
        }
    }
    std::ofstream output(cfg.memory_path, std::ios::app | std::ios::binary);
    if (!output.is_open()) {
        error = "cannot open memory file: " + cfg.memory_path.string();
        return false;
    }
    if (std::filesystem::exists(cfg.memory_path, ec) &&
        std::filesystem::file_size(cfg.memory_path, ec) > 0) {
        output << "\n";
    }
    // Escape newlines so each memory is one persistent line.
    for (const char ch : value) {
        if (ch == '\n' || ch == '\r') {
            output << ' ';
        } else {
            output << ch;
        }
    }
    output << "\n";
    if (!output) {
        error = "failed to write memory file";
        return false;
    }
    return true;
}

bool remove_memory(const Config& cfg,
                   const std::string& needle,
                   size_t& removed,
                   std::string& error) {
    removed = 0;
    const std::string target = lower(trim(needle));
    if (target.empty()) {
        error = "forget text is empty";
        return false;
    }
    std::ifstream input(cfg.memory_path);
    if (!input.is_open()) {
        return true;
    }
    std::vector<std::string> kept;
    std::string line;
    while (std::getline(input, line)) {
        if (lower(trim(line)).find(target) != std::string::npos) {
            removed += 1;
        } else {
            kept.push_back(line);
        }
    }
    input.close();
    if (removed == 0) {
        return true;
    }
    const auto parent = cfg.memory_path.parent_path();
    std::error_code ec;
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream output(cfg.memory_path, std::ios::trunc | std::ios::binary);
    if (!output.is_open()) {
        error = "cannot open memory file for writing";
        return false;
    }
    for (const auto& remaining : kept) {
        output << remaining << "\n";
    }
    if (!output) {
        error = "failed to write memory file";
        return false;
    }
    return true;
}

bool clear_memories(const Config& cfg, std::string& error) {
    std::error_code remove_error;
    std::filesystem::remove(cfg.memory_path, remove_error);
    std::ifstream probe(cfg.memory_path);
    if (probe.is_open()) {
        probe.close();
        error = "could not clear memory file: " + cfg.memory_path.string();
        return false;
    }
    return true;
}

int handle_session_command(Config& cfg,
                           const std::vector<std::string>& args,
                           std::string& error) {
    const std::string action = args.empty() ? "list" : args[0];
    if (action == "list" || action == "ls") {
        std::string list_error;
        const auto files = list_session_files(cfg, list_error);
        if (!list_error.empty()) {
            error = list_error;
            return 1;
        }
        if (files.empty()) {
            std::cout << "No saved sessions yet.\n";
            return 0;
        }
        std::cout << std::left;
        std::cout << std::setw(24) << "SESSION"
                  << std::setw(14) << "SIZE"
                  << "MODIFIED\n";
        std::cout << std::string(62, '-') << "\n";
        for (const auto& path : files) {
            std::error_code ec;
            const uint64_t size = static_cast<uint64_t>(std::filesystem::file_size(path, ec));
            const auto modified = std::filesystem::last_write_time(path, ec);
            (void)modified;
            std::cout << std::setw(24) << path.stem().string()
                      << std::setw(14) << to_human_bytes(size)
                      << path.filename().string() << "\n";
        }
        std::cout << "\nUse `skifflm --session <name>` or `skifflm session use <name>`.\n";
        return 0;
    }

    if (action == "show" || action == "info") {
        if (args.size() < 2) {
            error = "usage: skifflm session show <name>";
            return 2;
        }
        const std::filesystem::path path = session_file_for(cfg, args[1]);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            error = "session does not exist: " + args[1];
            return 1;
        }
        Config session_cfg = cfg;
        session_cfg.history_path = path;
        Session session(session_cfg);
        std::string session_error;
        if (!session.load(session_error)) {
            error = session_error;
            return 1;
        }
        std::cout << "Session: " << path.stem().string() << "\n";
        std::cout << "Path:    " << path.string() << "\n";
        std::cout << "Messages:" << session.message_count() << "\n";
        std::cout << "System:  " << (session.system_prompt().empty()
                                         ? std::string("(none)")
                                         : compact_preview(session.system_prompt())) << "\n";
        if (!session.empty()) {
            std::cout << "Last:    "
                      << compact_preview(session.conversation().back().content, 160) << "\n";
        }
        return 0;
    }

    if (action == "remove" || action == "rm" || action == "delete") {
        if (args.size() < 2) {
            error = "usage: skifflm session remove <name>";
            return 2;
        }
        const std::filesystem::path path = session_file_for(cfg, args[1]);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::cout << "Session " << args[1] << " is not saved.\n";
            return 0;
        }
        if (!std::filesystem::remove(path, ec) || ec) {
            error = "could not remove session: " + path.string();
            return 1;
        }
        std::cout << "Removed session " << args[1] << "\n";
        return 0;
    }

    if (action == "rename" || action == "mv") {
        if (args.size() < 3) {
            error = "usage: skifflm session rename <old> <new>";
            return 2;
        }
        const std::filesystem::path src = session_file_for(cfg, args[1]);
        const std::filesystem::path dst = session_file_for(cfg, args[2]);
        std::error_code ec;
        if (!std::filesystem::exists(src, ec)) {
            error = "session does not exist: " + args[1];
            return 1;
        }
        if (std::filesystem::exists(dst, ec)) {
            error = "a session already exists with the name: " + args[2];
            return 1;
        }
        std::filesystem::rename(src, dst, ec);
        if (ec) {
            error = "could not rename session: " + ec.message();
            return 1;
        }
        if (cfg.session_name == args[1]) {
            cfg.session_name = args[2];
        }
        std::cout << "Renamed session " << args[1] << " to " << args[2] << "\n";
        return 0;
    }

    if (action == "use" || action == "switch") {
        if (args.size() < 2) {
            error = "usage: skifflm session use <name>";
            return 2;
        }
        cfg.session_name = args[1];
        cfg.history_path = session_file_for(cfg, args[1]);
        cfg.one_shot.clear();
        cfg.interactive = true;
        return -1;  // continue into the normal model path
    }

    error = "unknown session action: " + action +
            " (use list, show, rename, remove or use)";
    return 2;
}

bool is_stdin_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

std::string read_stdin_all() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

}  // namespace skifflm
