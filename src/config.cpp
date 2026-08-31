#include "skifflm/config.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace skifflm {
namespace {

std::string home_directory() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || *home == '\0') {
        home = std::getenv("USERPROFILE");
    }
    if (home == nullptr) {
        return ".";
    }
    return home;
}

bool parse_int_value(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parse_float_value(const std::string& text, float& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<float>(parsed);
    return true;
}

bool parse_bool_value(const std::string& text, bool& value) {
    if (text.empty()) {
        return true;
    }
    const std::string normalized = lower(trim(text));
    if (normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on") {
        value = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off") {
        value = false;
        return true;
    }
    return false;
}

bool parse_uint_value(const std::string& text, uint32_t& value) {
    if (text.empty()) {
        return false;
    }
    if (text == "random" || text == "auto") {
        value = 0xFFFFFFFFu;
        return true;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

std::string option_value(int& index,
                         int argc,
                         char** argv,
                         const std::string& name,
                         std::string& error) {
    const std::string prefix = name + "=";
    const std::string current(argv[index]);
    if (current.size() > prefix.size() && current.compare(0, prefix.size(), prefix) == 0) {
        return current.substr(prefix.size());
    }
    if (index + 1 >= argc) {
        error = "missing value for " + name;
        return {};
    }
    index += 1;
    return argv[index];
}

bool apply_key_value(Config& cfg,
                     const std::string& key,
                     const std::string& value,
                     std::string& error) {
    auto set_flag = [&error](bool& target,
                             bool default_value,
                             bool invert,
                             const std::string& raw,
                             const std::string& option_name) -> bool {
        bool enabled = default_value;
        if (!raw.empty()) {
            if (!parse_bool_value(raw, enabled)) {
                error = "invalid boolean for --" + option_name + ": " + raw;
                return false;
            }
        }
        target = invert ? !enabled : enabled;
        return true;
    };

    if (key == "model") {
        cfg.model_path = expand_path(value);
    } else if (key == "model-dir") {
        cfg.model_dir = expand_path(value);
    } else if (key == "config") {
        cfg.config_path = expand_path(value);
    } else if (key == "history") {
        cfg.history_path = expand_path(value);
    } else if (key == "system" || key == "system-prompt") {
        cfg.system_prompt = value;
    } else if (key == "ctx" || key == "context") {
        if (!parse_int_value(value, cfg.context_size)) {
            error = "invalid integer for --ctx: " + value;
            return false;
        }
        if (cfg.context_size < 64) {
            error = "context size must be at least 64";
            return false;
        }
    } else if (key == "batch") {
        if (!parse_int_value(value, cfg.batch_size)) {
            error = "invalid integer for --batch: " + value;
            return false;
        }
        if (cfg.batch_size < 1) {
            error = "batch size must be at least 1";
            return false;
        }
    } else if (key == "threads") {
        if (!parse_int_value(value, cfg.n_threads)) {
            error = "invalid integer for --threads: " + value;
            return false;
        }
        if (cfg.n_threads < 0) {
            error = "threads must be zero or positive";
            return false;
        }
    } else if (key == "gpu-layers") {
        if (!parse_int_value(value, cfg.n_gpu_layers)) {
            error = "invalid integer for --gpu-layers: " + value;
            return false;
        }
    } else if (key == "temp" || key == "temperature") {
        if (!parse_float_value(value, cfg.temperature)) {
            error = "invalid number for --temp: " + value;
            return false;
        }
        if (cfg.temperature < 0.0f) {
            error = "temperature must be zero or positive";
            return false;
        }
    } else if (key == "top-p") {
        if (!parse_float_value(value, cfg.top_p)) {
            error = "invalid number for --top-p: " + value;
            return false;
        }
        if (cfg.top_p <= 0.0f || cfg.top_p > 1.0f) {
            error = "top-p must be in (0, 1]";
            return false;
        }
    } else if (key == "top-k") {
        if (!parse_int_value(value, cfg.top_k)) {
            error = "invalid integer for --top-k: " + value;
            return false;
        }
    } else if (key == "repeat-penalty") {
        if (!parse_float_value(value, cfg.repeat_penalty)) {
            error = "invalid number for --repeat-penalty: " + value;
            return false;
        }
        if (cfg.repeat_penalty <= 0.0f) {
            error = "repeat-penalty must be positive";
            return false;
        }
    } else if (key == "repeat-last-n") {
        if (!parse_int_value(value, cfg.repeat_last_n)) {
            error = "invalid integer for --repeat-last-n: " + value;
            return false;
        }
        if (cfg.repeat_last_n < 0) {
            error = "repeat-last-n must be zero or positive";
            return false;
        }
    } else if (key == "n" || key == "n-predict") {
        if (!parse_int_value(value, cfg.n_predict)) {
            error = "invalid integer for --n-predict: " + value;
            return false;
        }
        if (cfg.n_predict < 1) {
            error = "n-predict must be at least 1";
            return false;
        }
    } else if (key == "seed") {
        if (!parse_uint_value(value, cfg.seed)) {
            error = "invalid value for --seed: " + value;
            return false;
        }
    } else if (key == "prompt-file") {
        cfg.prompt_file = expand_path(value);
    } else if (key == "color") {
        if (!set_flag(cfg.color, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-color") {
        if (!set_flag(cfg.color, true, true, value, key)) {
            return false;
        }
    } else if (key == "mmap") {
        if (!set_flag(cfg.use_mmap, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-mmap") {
        if (!set_flag(cfg.use_mmap, true, true, value, key)) {
            return false;
        }
    } else if (key == "mlock") {
        if (!set_flag(cfg.use_mlock, true, false, value, key)) {
            return false;
        }
    } else if (key == "save") {
        if (!set_flag(cfg.save_history, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-save") {
        if (!set_flag(cfg.save_history, true, true, value, key)) {
            return false;
        }
    } else if (key == "interactive") {
        if (!set_flag(cfg.interactive, true, false, value, key)) {
            return false;
        }
    } else if (key == "non-interactive") {
        if (!set_flag(cfg.interactive, true, true, value, key)) {
            return false;
        }
    } else if (key == "verbose") {
        if (!set_flag(cfg.log_llama, true, false, value, key)) {
            return false;
        }
    } else if (key == "quiet") {
        if (!set_flag(cfg.log_llama, true, true, value, key)) {
            return false;
        }
    } else if (key == "info") {
        if (!set_flag(cfg.show_info, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-info") {
        if (!set_flag(cfg.show_info, true, true, value, key)) {
            return false;
        }
    } else if (key == "stdin") {
        bool enabled = true;
        if (!value.empty() && !parse_bool_value(value, enabled)) {
            error = "invalid boolean for --stdin: " + value;
            return false;
        }
        cfg.read_stdin = enabled;
        cfg.interactive = !enabled;
    } else if (key == "prompt") {
        cfg.one_shot = value;
        cfg.interactive = false;
    } else if (key == "reset-history") {
        if (!set_flag(cfg.reset_history, true, false, value, key)) {
            return false;
        }
    } else if (key == "help") {
        if (!set_flag(cfg.show_help, true, false, value, key)) {
            return false;
        }
    } else if (key == "version") {
        if (!set_flag(cfg.show_version, true, false, value, key)) {
            return false;
        }
    } else if (key == "show-config") {
        if (!set_flag(cfg.show_config, true, false, value, key)) {
            return false;
        }
    } else if (key == "debug") {
        if (!set_flag(cfg.debug, true, false, value, key)) {
            return false;
        }
    } else {
        error = "unknown option: " + key;
        return false;
    }
    return true;
}

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() &&
           text.compare(0, prefix.size(), prefix) == 0;
}

}

Config default_config() {
    Config cfg;
    const std::string home = home_directory();
    cfg.model_dir = expand_path(home + "/.local/share/skifflm/models");
    cfg.config_path = expand_path(home + "/.config/skifflm/config");
    cfg.history_path = expand_path(home + "/.local/share/skifflm/history.skif");
    return cfg;
}

std::filesystem::path expand_path(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    if (value == "~") {
        return std::filesystem::path(home_directory());
    }
    if (value.size() > 2 && value[0] == '~' && (value[1] == '/' || value[1] == '\\')) {
        return std::filesystem::path(home_directory()) / value.substr(2);
    }
    return std::filesystem::path(value);
}

std::string trim(const std::string& value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string to_human_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit += 1;
    }
    std::ostringstream out;
    if (unit == 0) {
        out << static_cast<uint64_t>(value) << " " << units[unit];
    } else {
        out << std::fixed << std::setprecision(2) << value << " " << units[unit];
    }
    return out.str();
}

std::string to_human_count(uint64_t count) {
    const char* units[] = {"", "K", "M", "B", "T"};
    double value = static_cast<double>(count);
    int unit = 0;
    while (value >= 1000.0 && unit < 4) {
        value /= 1000.0;
        unit += 1;
    }
    std::ostringstream out;
    if (unit == 0) {
        out << count;
    } else {
        out << std::fixed << std::setprecision(2) << value << " " << units[unit];
    }
    return out.str();
}

std::vector<std::filesystem::path> discover_models(const Config& cfg, std::string& error) {
    std::vector<std::filesystem::path> paths;
    std::vector<std::filesystem::path> roots;

    if (!cfg.model_path.empty()) {
        paths.push_back(cfg.model_path);
        return paths;
    }

    roots.push_back(cfg.model_dir);
    roots.push_back(std::filesystem::path("models"));

    for (const auto& root : roots) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) {
            continue;
        }
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec); it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                break;
            }
            if (it->is_regular_file() && it->path().extension() == ".gguf") {
                paths.push_back(it->path());
            }
        }
    }

    if (paths.empty()) {
        error = "no GGUF model found in the model directory: " + cfg.model_dir.string();
        return {};
    }

    return paths;
}

bool parse_config_file(const std::filesystem::path& path, Config& cfg, std::string& error) {
    if (path.empty()) {
        return true;
    }
    std::ifstream input(path);
    if (!input.is_open()) {
        if (std::filesystem::exists(path)) {
            error = "cannot open config file: " + path.string();
            return false;
        }
        return true;
    }

    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        line_number += 1;
        const std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned[0] == '#') {
            continue;
        }
        const size_t eq = cleaned.find('=');
        std::string key;
        std::string value;
        if (eq == std::string::npos) {
            key = lower(trim(cleaned));
            value = {};
        } else {
            key = lower(trim(cleaned.substr(0, eq)));
            value = trim(cleaned.substr(eq + 1));
        }
        if (!apply_key_value(cfg, key, value, error)) {
            error = "config line " + std::to_string(line_number) + ": " + error;
            return false;
        }
    }
    return true;
}

bool parse_args(int argc, char** argv, Config& cfg, std::string& error) {
    if (argc < 2) {
        return true;
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            cfg.show_help = true;
            continue;
        }
        if (arg == "--version" || arg == "-v") {
            cfg.show_version = true;
            continue;
        }
        if (arg == "--show-config") {
            cfg.show_config = true;
            continue;
        }
        if (!starts_with(arg, "--")) {
            if (cfg.model_path.empty()) {
                cfg.model_path = expand_path(arg);
                continue;
            }
            error = "unexpected positional argument: " + arg;
            return false;
        }

        const std::string name = lower(arg.substr(2));
        const size_t eq = name.find('=');
        const std::string key = eq == std::string::npos ? name : name.substr(0, eq);

        if (key == "help" || key == "version" || key == "show-config" ||
            key == "no-color" || key == "mmap" || key == "no-mmap" ||
            key == "mlock" || key == "save" || key == "no-save" ||
            key == "interactive" || key == "non-interactive" ||
            key == "verbose" || key == "quiet" || key == "info" ||
            key == "no-info" || key == "stdin" || key == "reset-history" ||
            key == "debug" || key == "color") {
            const std::string value = eq == std::string::npos ? std::string() : name.substr(eq + 1);
            const std::string option_key = key;
            if (!value.empty()) {
                if (key == "stdin" || key == "verbose" || key == "no-color" || key == "no-mmap" ||
                    key == "mlock" || key == "no-save" || key == "non-interactive" ||
                    key == "reset-history" || key == "debug" || key == "quiet" ||
                    key == "no-info" || key == "interactive" || key == "save" ||
                    key == "mmap" || key == "color") {
                    error = "option --" + key + " does not take a value";
                    return false;
                }
            }
            if (!apply_key_value(cfg, option_key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "model" || key == "model-dir" || key == "config" ||
            key == "history" || key == "system" || key == "system-prompt" ||
            key == "prompt" || key == "prompt-file") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "ctx" || key == "context" || key == "batch" || key == "threads" ||
            key == "gpu-layers" || key == "top-k" || key == "repeat-last-n" ||
            key == "n" || key == "n-predict" || key == "seed") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "temp" || key == "temperature" || key == "top-p" ||
            key == "repeat-penalty") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        error = "unknown option: " + arg;
        return false;
    }
    return true;
}

void apply_environment(Config& cfg) {
    if (const char* value = std::getenv("SKIFFLLM_MODEL")) {
        cfg.model_path = expand_path(value);
    }
    if (const char* value = std::getenv("SKIFFLLM_MODEL_DIR")) {
        cfg.model_dir = expand_path(value);
    }
    if (const char* value = std::getenv("SKIFFLLM_CONFIG")) {
        cfg.config_path = expand_path(value);
    }
    if (const char* value = std::getenv("SKIFFLLM_HISTORY")) {
        cfg.history_path = expand_path(value);
    }
    if (const char* value = std::getenv("SKIFFLLM_SYSTEM")) {
        cfg.system_prompt = value;
    }
}

std::string usage(const std::string& program) {
    std::ostringstream out;
    out << "SkiffLLM - A fully offline local LLM terminal assistant built on llama.cpp\n\n";
    out << "Usage: " << program << " [options] [model.gguf]\n\n";
    out << "Model options:\n";
    out << "  --model <path>             Path to a GGUF model file\n";
    out << "  --model-dir <path>         Directory scanned for a GGUF model\n";
    out << "  --ctx <n>                  Context size (default: 4096)\n";
    out << "  --batch <n>                Batch size (default: 512)\n";
    out << "  --threads <n>              CPU threads (0 means auto)\n";
    out << "  --gpu-layers <n>           GPU layers to offload (-1 means all)\n";
    out << "  --mmap                     Enable mmap (default)\n";
    out << "  --no-mmap                  Disable mmap\n";
    out << "  --mlock                    Lock model memory\n\n";
    out << "Generation options:\n";
    out << "  --temp <t>                 Sampling temperature (default: 0.70)\n";
    out << "  --top-p <p>                Nucleus sampling (default: 0.95)\n";
    out << "  --top-k <n>                Top-K sampling (default: 40)\n";
    out << "  --repeat-penalty <p>       Repeat penalty (default: 1.10)\n";
    out << "  --repeat-last-n <n>        Repeat penalty window (default: 64)\n";
    out << "  --n-predict <n>            Max generated tokens (default: 512)\n";
    out << "  --seed <n|random>          Sampling seed (default: random)\n\n";
    out << "Session options:\n";
    out << "  --system <text>            System prompt\n";
    out << "  --history <path>           Session history file\n";
    out << "  --reset-history            Ignore and overwrite saved history\n";
    out << "  --no-save                  Do not persist the session\n\n";
    out << "Program options:\n";
    out << "  --prompt <text>            Single prompt mode\n";
    out << "  --prompt-file <path>       Read the single prompt from a file\n";
    out << "  --stdin                    Read the single prompt from stdin\n";
    out << "  --non-interactive          Disable the interactive shell\n";
    out << "  --color                    Force ANSI colors (default: auto)\n";
    out << "  --no-color                 Disable ANSI colors\n";
    out << "  --verbose                  Print llama.cpp logs\n";
    out << "  --config <path>            Config file path\n";
    out << "  --show-config              Print the effective configuration\n";
    out << "  --help                     Show this help\n";
    out << "  --version                  Show the version\n";
    out << "\nInteractive commands: /help /info /history /clear /reset /system /model /temp /top-p /top-k /n /save /exit\n";
    return out.str();
}

void print_config(const Config& cfg) {
    std::cout << "model            " << (cfg.model_path.empty() ? "(auto)" : cfg.model_path.string()) << "\n";
    std::cout << "model_dir        " << cfg.model_dir.string() << "\n";
    std::cout << "config_path      " << cfg.config_path.string() << "\n";
    std::cout << "history_path     " << cfg.history_path.string() << "\n";
    std::cout << "system_prompt    " << (cfg.system_prompt.empty() ? "(none)" : cfg.system_prompt) << "\n";
    std::cout << "context_size     " << cfg.context_size << "\n";
    std::cout << "batch_size       " << cfg.batch_size << "\n";
    std::cout << "threads          " << (cfg.n_threads == 0 ? "auto" : std::to_string(cfg.n_threads)) << "\n";
    std::cout << "gpu_layers       " << cfg.n_gpu_layers << "\n";
    std::cout << "temperature      " << cfg.temperature << "\n";
    std::cout << "top_p            " << cfg.top_p << "\n";
    std::cout << "top_k            " << cfg.top_k << "\n";
    std::cout << "repeat_penalty   " << cfg.repeat_penalty << "\n";
    std::cout << "repeat_last_n    " << cfg.repeat_last_n << "\n";
    std::cout << "n_predict        " << cfg.n_predict << "\n";
    std::cout << "seed             " << (cfg.seed == 0xFFFFFFFFu ? "random" : std::to_string(cfg.seed)) << "\n";
    std::cout << "use_mmap         " << (cfg.use_mmap ? "yes" : "no") << "\n";
    std::cout << "use_mlock        " << (cfg.use_mlock ? "yes" : "no") << "\n";
    std::cout << "color            " << (cfg.color ? "yes" : "no") << "\n";
    std::cout << "interactive      " << (cfg.interactive ? "yes" : "no") << "\n";
    std::cout << "save_history     " << (cfg.save_history ? "yes" : "no") << "\n";
}

}
