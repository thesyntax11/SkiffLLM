#include "skiffllm/config.hpp"
#include "skiffllm/skills.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace skiffllm {
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

std::string option_value(int& index, int argc, char** argv, const std::string& name,
                         std::string& error) {
    const std::string prefix = name + "=";
    const std::string current(argv[index]);
    if (current.size() >= prefix.size() && current.compare(0, prefix.size(), prefix) == 0) {
        return current.substr(prefix.size());
    }
    if (index + 1 >= argc) {
        error = "missing value for " + name;
        return {};
    }
    index += 1;
    return argv[index];
}

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with_ci(const std::string& text, const std::string& suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    const size_t offset = text.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char a =
            static_cast<char>(std::tolower(static_cast<unsigned char>(text[offset + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) {
            return false;
        }
    }
    return true;
}

bool looks_like_model_path(const std::string& value) {
    if (ends_with_ci(value, ".gguf")) {
        return true;
    }
    if (value.find('/') == std::string::npos && value.find('\\') == std::string::npos) {
        return false;
    }
    return std::filesystem::exists(expand_path(value));
}

std::string sanitize_session(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (const unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('_');
        }
    }
    const std::string cleaned = trim(result);
    return cleaned.empty() ? "default" : cleaned;
}

std::filesystem::path resolve_session_path(const std::string& name,
                                           const std::filesystem::path& parent) {
    if (!name.empty()) {
        const bool has_separator =
            name.find('/') != std::string::npos || name.find('\\') != std::string::npos;
        const bool has_skif = name.size() > 5 && name.compare(name.size() - 5, 5, ".skif") == 0;
        if (has_separator || has_skif) {
            return expand_path(name);
        }
    }

    std::filesystem::path base = parent;
    if (base.empty()) {
        const std::string home = home_directory();
        base = std::filesystem::path(home) / ".local/share/skiffllm";
    }
    return base / (sanitize_session(name) + ".skif");
}

}

Config default_config() {
    Config cfg;
    const std::string home = home_directory();
    cfg.model_dir = expand_path(home + "/.local/share/skiffllm/models");
    cfg.config_path = expand_path(home + "/.config/skiffllm/config");
    cfg.history_path = expand_path(home + "/.local/share/skiffllm/history.skif");
    cfg.memory_path = expand_path(home + "/.local/share/skiffllm/memories.txt");
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
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
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

    std::sort(paths.begin(), paths.end());
    return paths;
}

bool apply_profile(Config& cfg, const std::string& name, std::string& error) {
    const std::string normalized = lower(trim(name));
    cfg.profile_name = normalized;
    if (normalized.empty()) {
        return true;
    }

    if (normalized == "balanced") {
        cfg.temperature = 0.70f;
        cfg.top_p = 0.95f;
        cfg.top_k = 40;
        cfg.min_p = 0.00f;
        cfg.typical_p = 0.00f;
        cfg.repeat_penalty = 1.10f;
        cfg.n_predict = 512;
        return true;
    }
    if (normalized == "fast") {
        cfg.temperature = 0.60f;
        cfg.top_p = 0.90f;
        cfg.top_k = 30;
        cfg.min_p = 0.00f;
        cfg.typical_p = 0.00f;
        cfg.repeat_penalty = 1.05f;
        cfg.n_predict = 256;
        return true;
    }
    if (normalized == "creative") {
        cfg.temperature = 1.00f;
        cfg.top_p = 0.98f;
        cfg.top_k = 60;
        cfg.min_p = 0.05f;
        cfg.typical_p = 0.00f;
        cfg.repeat_penalty = 1.20f;
        cfg.n_predict = 512;
        return true;
    }
    if (normalized == "code") {
        cfg.temperature = 0.20f;
        cfg.top_p = 0.90f;
        cfg.top_k = 20;
        cfg.min_p = 0.00f;
        cfg.typical_p = 0.00f;
        cfg.repeat_penalty = 1.20f;
        cfg.n_predict = 1024;
        return true;
    }
    if (normalized == "precise") {
        cfg.temperature = 0.00f;
        cfg.top_p = 0.90f;
        cfg.top_k = 20;
        cfg.min_p = 0.00f;
        cfg.typical_p = 0.00f;
        cfg.repeat_penalty = 1.00f;
        cfg.n_predict = 650;
        return true;
    }

    error = "unknown profile: " + name + " (use balanced, fast, creative, code or precise)";
    return false;
}

bool apply_key_value(Config& cfg, const std::string& key, const std::string& value,
                     std::string& error) {
    auto set_flag = [&error](bool& target, bool default_value, bool invert, const std::string& raw,
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
    } else if (key == "session") {
        cfg.session_name = value;
        cfg.history_path = resolve_session_path(value, cfg.history_path.parent_path());
    } else if (key == "profile") {
        if (!apply_profile(cfg, value, error)) {
            return false;
        }
    } else if (key == "stop") {
        const std::string stop = trim(value);
        if (!stop.empty()) {
            cfg.stop_sequences.push_back(stop);
        }
    } else if (key == "output") {
        cfg.output_path = expand_path(value);
    } else if (key == "export") {
        cfg.export_path = expand_path(value);
    } else if (key == "attach" || key == "file") {
        const std::filesystem::path path = expand_path(value);
        if (!path.empty()) {
            cfg.attach_paths.push_back(path);
        }
    } else if (key == "chat-template") {
        cfg.chat_template = value;
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
    } else if (key == "ubatch") {
        if (!parse_int_value(value, cfg.n_ubatch)) {
            error = "invalid integer for --ubatch: " + value;
            return false;
        }
        if (cfg.n_ubatch < 0) {
            error = "ubatch must be zero or positive";
            return false;
        }
    } else if (key == "reserve-ctx") {
        if (!parse_int_value(value, cfg.reserve_ctx)) {
            error = "invalid integer for --reserve-ctx: " + value;
            return false;
        }
        if (cfg.reserve_ctx < 0) {
            error = "reserve-ctx must be zero or positive";
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
    } else if (key == "min-p") {
        if (!parse_float_value(value, cfg.min_p)) {
            error = "invalid number for --min-p: " + value;
            return false;
        }
        if (cfg.min_p < 0.0f || cfg.min_p > 1.0f) {
            error = "min-p must be in [0, 1]";
            return false;
        }
    } else if (key == "typical") {
        if (!parse_float_value(value, cfg.typical_p)) {
            error = "invalid number for --typical: " + value;
            return false;
        }
        if (cfg.typical_p < 0.0f || cfg.typical_p > 1.0f) {
            error = "typical must be in [0, 1]";
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
    } else if (key == "project") {
        cfg.project_path = expand_path(value);
        cfg.interactive = false;
    } else if (key == "summarize") {
        cfg.summarize_path = expand_path(value);
        cfg.interactive = false;
    } else if (key == "remember") {
        cfg.remember_text = trim(value);
    } else if (key == "forget") {
        cfg.forget_text = trim(value);
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
        cfg.show_info = false;
    } else if (key == "info") {
        if (!set_flag(cfg.show_info, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-info" || key == "no-banner") {
        if (!set_flag(cfg.show_info, true, true, value, key)) {
            return false;
        }
    } else if (key == "auto-trim") {
        if (!set_flag(cfg.auto_trim, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-auto-trim") {
        if (!set_flag(cfg.auto_trim, true, true, value, key)) {
            return false;
        }
    } else if (key == "json") {
        if (!set_flag(cfg.json_output, true, false, value, key)) {
            return false;
        }
        if (cfg.json_output) {
            cfg.interactive = false;
        }
    } else if (key == "list-models") {
        if (!set_flag(cfg.list_models, true, false, value, key)) {
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
    } else if (key == "flash-attn") {
        if (!set_flag(cfg.flash_attn, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-flash-attn") {
        if (!set_flag(cfg.flash_attn, true, true, value, key)) {
            return false;
        }
    } else if (key == "numa") {
        if (!set_flag(cfg.numa, true, false, value, key)) {
            return false;
        }
    } else if (key == "kv-offload") {
        if (!set_flag(cfg.offload_kqv, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-kv-offload") {
        if (!set_flag(cfg.offload_kqv, true, true, value, key)) {
            return false;
        }
    } else if (key == "n-keep") {
        if (!parse_int_value(value, cfg.n_keep)) {
            error = "invalid integer for --n-keep: " + value;
            return false;
        }
        if (cfg.n_keep < 0) {
            error = "n-keep must be zero or positive";
            return false;
        }
    } else if (key == "doctor") {
        if (!set_flag(cfg.doctor, true, false, value, key)) {
            return false;
        }
    } else if (key == "code") {
        if (!set_flag(cfg.code_mode, true, false, value, key)) {
            return false;
        }
        cfg.interactive = false;
    } else if (key == "network") {
        if (!set_flag(cfg.doctor_network, true, false, value, key)) {
            return false;
        }
    } else if (key == "model-info") {
        if (!set_flag(cfg.model_info, true, false, value, key)) {
            return false;
        }
        if (cfg.model_info) {
            cfg.interactive = false;
        }
    } else if (key == "smoke") {
        if (!set_flag(cfg.smoke, true, false, value, key)) {
            return false;
        }
        if (cfg.smoke) {
            cfg.interactive = false;
            if (cfg.one_shot.empty() && cfg.prompt_file.empty() && !cfg.read_stdin) {
                cfg.one_shot = "Reply with OK.";
            }
        }
    } else if (key == "warmup") {
        if (!set_flag(cfg.warmup, true, false, value, key)) {
            return false;
        }
    } else if (key == "tokenize") {
        cfg.tokenize_text = value;
        cfg.interactive = false;
    } else if (key == "serve") {
        if (!set_flag(cfg.serve, true, false, value, key)) {
            return false;
        }
        if (cfg.serve) {
            cfg.interactive = false;
        }
    } else if (key == "host") {
        cfg.server_host = value;
    } else if (key == "port") {
        if (!parse_int_value(value, cfg.server_port)) {
            error = "invalid integer for --port: " + value;
            return false;
        }
        if (cfg.server_port < 1 || cfg.server_port > 65535) {
            error = "port must be between 1 and 65535";
            return false;
        }
    } else if (key == "api-key" || key == "key") {
        cfg.api_key = value;
    } else if (key == "benchmark") {
        if (!parse_int_value(value, cfg.benchmark_runs)) {
            error = "invalid integer for --benchmark: " + value;
            return false;
        }
        if (cfg.benchmark_runs < 0) {
            error = "benchmark runs must be zero or positive";
            return false;
        }
        if (cfg.benchmark_runs > 0) {
            cfg.interactive = false;
        }
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
    } else if (key == "context-bar") {
        if (!set_flag(cfg.context_bar, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-context-bar") {
        if (!set_flag(cfg.context_bar, true, true, value, key)) {
            return false;
        }
    } else if (key == "backend-info") {
        if (!set_flag(cfg.backend_info, true, false, value, key)) {
            return false;
        }
    } else if (key == "skills") {
        if (!set_flag(cfg.skills_enabled, true, false, value, key)) {
            return false;
        }
    } else if (key == "no-skills") {
        if (!set_flag(cfg.skills_enabled, true, true, value, key)) {
            return false;
        }
    } else if (key == "enable-skill" || key == "disable-skill") {
        const std::string skill = lower(trim(value));
        if (skill.empty()) {
            error = "skill name cannot be empty";
            return false;
        }
        const auto catalog = skiffllm::skill_catalog();
        if (std::find(catalog.begin(), catalog.end(), skill) == catalog.end()) {
            error = "unknown skill: " + skill + " (use skill list)";
            return false;
        }
        const auto found = std::find(cfg.enabled_skills.begin(), cfg.enabled_skills.end(), skill);
        if (key == "enable-skill") {
            if (found == cfg.enabled_skills.end()) {
                cfg.enabled_skills.push_back(skill);
            }
        } else if (found != cfg.enabled_skills.end()) {
            cfg.enabled_skills.erase(found);
        }
    } else {
        error = "unknown option: " + key;
        return false;
    }
    return true;
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
            if (cfg.model_path.empty() && looks_like_model_path(arg)) {
                cfg.model_path = expand_path(arg);
                continue;
            }
            if (!cfg.one_shot.empty()) {
                cfg.one_shot += " " + arg;
            } else {
                cfg.one_shot = arg;
            }
            cfg.interactive = false;
            continue;
        }

        const std::string name = lower(arg.substr(2));
        const size_t eq = name.find('=');
        const std::string key = eq == std::string::npos ? name : name.substr(0, eq);
        const bool has_value = eq != std::string::npos;

        if (key == "help" || key == "version" || key == "show-config" || key == "color" ||
            key == "no-color" || key == "mmap" || key == "no-mmap" || key == "mlock" ||
            key == "save" || key == "no-save" || key == "interactive" || key == "non-interactive" ||
            key == "verbose" || key == "quiet" || key == "info" || key == "no-info" ||
            key == "no-banner" || key == "stdin" || key == "reset-history" || key == "debug" ||
            key == "auto-trim" || key == "no-auto-trim" || key == "json" || key == "list-models" ||
            key == "flash-attn" || key == "no-flash-attn" || key == "numa" || key == "kv-offload" ||
            key == "no-kv-offload" || key == "doctor" || key == "network" || key == "code" ||
            key == "model-info" || key == "smoke" || key == "warmup" || key == "serve" ||
            key == "context-bar" || key == "no-context-bar" || key == "backend-info" ||
            key == "skills" || key == "no-skills") {
            if (has_value) {
                error = "option --" + key + " does not take a value";
                return false;
            }
            if (!apply_key_value(cfg, key, {}, error)) {
                return false;
            }
            continue;
        }

        if (key == "model" || key == "model-dir" || key == "config" || key == "history" ||
            key == "system" || key == "system-prompt" || key == "session" || key == "profile" ||
            key == "stop" || key == "output" || key == "export" || key == "attach" ||
            key == "file" || key == "chat-template" || key == "prompt" || key == "prompt-file" ||
            key == "project" || key == "summarize" || key == "remember" || key == "forget" ||
            key == "tokenize" || key == "host" || key == "port" || key == "api-key" ||
            key == "key" || key == "benchmark" || key == "enable-skill" || key == "disable-skill") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "ctx" || key == "context" || key == "batch" || key == "ubatch" ||
            key == "reserve-ctx" || key == "threads" || key == "gpu-layers" || key == "top-k" ||
            key == "repeat-last-n" || key == "n" || key == "n-predict" || key == "seed" ||
            key == "n-keep") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "temp" || key == "temperature" || key == "top-p" || key == "min-p" ||
            key == "typical" || key == "repeat-penalty") {
            const std::string value = option_value(i, argc, argv, "--" + key, error);
            if (!error.empty()) {
                return false;
            }
            if (!apply_key_value(cfg, key, value, error)) {
                return false;
            }
            continue;
        }

        if (key == "base-url" || key == "base" || key == "max-tokens") {
            if (!has_value) {
                const std::string value = option_value(i, argc, argv, "--" + key, error);
                if (!error.empty()) {
                    return false;
                }
            }
            continue;
        }
        if (key == "stream" || key == "no-json") {
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
    if (const char* value = std::getenv("SKIFFLLM_PROFILE")) {
        std::string error;
        apply_profile(cfg, value, error);
    }
    if (const char* value = std::getenv("SKIFFLLM_API_KEY")) {
        cfg.api_key = value;
    } else if (const char* value = std::getenv("SKIFFLLM_SERVER_KEY")) {
        cfg.api_key = value;
    }
}

std::string usage(const std::string& program) {
    std::ostringstream out;
    out << "SkiffLLM - An offline-first local LLM terminal assistant built on llama.cpp\n\n";
    out << "Usage: " << program << " [options] [model.gguf]\n\n";
    out << "Core options:\n";
    out << "  --model <path>                Path to a GGUF model file\n";
    out << "  --model-dir <path>            Directory scanned for a GGUF model\n";
    out << "  --list-models                 Print discovered GGUF models\n";
    out << "  --model-info                  Print model metadata and exit\n";
    out << "  --smoke                       Run a quick generation smoke test\n";
    out << "  --warmup                      Warm the model before the first answer\n";
    out << "  --code                        Propose concrete code edits (never applies them)\n";
    out << "  --doctor                      Print system and privacy diagnostics\n";
    out << "  --network                     With --doctor, show network/privacy facts\n";
    out << "  --tokenize <text>             Tokenize text and print token counts\n";
    out << "  --profile <name>              balanced, fast, creative, code or precise\n";
    out << "  --session <name>              Use a named conversation\n";
    out << "  --system <text>               System prompt (alias: --system-prompt)\n";
    out << "  --stop <text>                 Stop sequence; can be repeated\n";
    out << "  --attach <path>               Attach a file; can be repeated\n";
    out << "  --file <path>                 Alias for --attach; can be repeated\n";
    out << "  --chat-template <name>        Override the model chat template\n";
    out << "  --export <path>               Export the loaded conversation as Markdown\n";
    out << "  --serve                       Serve a local OpenAI-compatible API\n";
    out << "  --host <addr>                 Local server bind address (default: 127.0.0.1)\n";
    out << "  --port <n>                    Local server port (default: 8080)\n";
    out << "  --api-key <key>               Require Bearer auth on the local server\n";
    out << "  --benchmark <runs>            Run a real generation benchmark\n\n";
    out << "Inference options:\n";
    out << "  --ctx <n>                     Context size (default: 4096)\n";
    out << "  --batch <n>                   Batch size (default: 512)\n";
    out << "  --ubatch <n>                  Physical batch size (default: auto)\n";
    out << "  --threads <n>                 CPU threads (0 means auto)\n";
    out << "  --gpu-layers <n>              GPU layers to offload (-1 means all)\n";
    out << "  --mmap                        Enable mmap (default)\n";
    out << "  --no-mmap                     Disable mmap\n";
    out << "  --mlock                       Lock model memory\n";
    out << "  --flash-attn                  Enable flash attention\n";
    out << "  --no-flash-attn               Disable flash attention\n";
    out << "  --numa                        Initialize NUMA optimization\n";
    out << "  --kv-offload                  Offload KV cache to device (default)\n";
    out << "  --no-kv-offload               Keep KV cache on CPU\n\n";

    out << "Sampling options:\n";
    out << "  --temp <t>                    Sampling temperature (default: 0.70; alias: "
           "--temperature)\n";
    out << "  --top-p <p>                   Nucleus sampling (default: 0.95)\n";
    out << "  --top-k <n>                   Top-K sampling (default: 40)\n";
    out << "  --min-p <p>                   Minimum probability filter\n";
    out << "  --typical <p>                 Locally typical sampling\n";
    out << "  --repeat-penalty <p>          Repeat penalty (default: 1.10)\n";
    out << "  --repeat-last-n <n>           Repeat penalty window (default: 64)\n";
    out << "  --n-predict <n>               Max generated tokens (default: 512; alias: --n)\n";
    out << "  --seed <n|random>             Sampling seed (default: random)\n\n";
    out << "Session options:\n";
    out << "  --history <path>              Session history file\n";
    out << "  --reset-history               Ignore and overwrite saved history\n";
    out << "  --save                        Persist the session (default)\n";
    out << "  --no-save                     Do not persist the session\n";
    out << "  --auto-trim                   Trim old history when context is full (default)\n";
    out << "  --no-auto-trim                Fail instead of trimming\n";
    out << "  --reserve-ctx <n>             Reserve tokens for generation\n";
    out << "  --n-keep <n>                  Keep at least n turns during trimming\n\n";

    out << "Program options:\n";
    out << "  --prompt <text>               Single prompt mode\n";
    out << "  --prompt-file <path>          Read the single prompt from a file\n";
    out << "  --stdin                       Read the single prompt from stdin\n";
    out << "  --project <dir>               Add a bounded project context block\n";
    out << "  --summarize <file>            Summarize a file (content context)\n";
    out << "  --remember <text>             Save a persistent memory line\n";
    out << "  --forget <text>               Remove matching persistent memories\n";
    out << "  --json                        Machine-readable JSON output\n";
    out << "  --output <path>               Write the text answer to a file\n";
    out << "  --non-interactive             Disable the interactive shell\n";
    out << "  --color                       Force ANSI colors (default: auto)\n";
    out << "  --no-color                    Disable ANSI colors\n";
    out << "  --no-banner                   Hide the startup banner\n";
    out << "  --quiet                       Suppress banners and llama logs\n";
    out << "  --verbose                     Print llama.cpp logs\n";
    out << "  --config <path>               Config file path\n";
    out << "  --show-config                 Print the effective configuration (JSON with --json)\n";
    out << "  --context-bar                 Show the live context usage bar (default)\n";
    out << "  --no-context-bar              Hide the context usage bar\n";
    out << "  --backend-info                Print the active llama.cpp backends and exit\n";
    out << "  --skills                      Allow the model to run enabled skills\n";
    out << "  --no-skills                   Keep the model text-only (default)\n";
    out << "  --enable-skill <name>         Enable a skill for automatic calls\n";
    out << "  --disable-skill <name>        Remove a skill from automatic calls\n";
    out << "  --help                        Show this help\n";
    out << "  --version                     Show the version\n";
    out << "\nSubcommands:\n";
    out << "  run [prompt] [opts]        One-shot prompt (e.g. `skiffllm run \"Merhaba\" --ctx "
           "2048 "
           "--temp 0.3 --threads 4`)\n";
    out << "  model list|info|install|remove|verify\n";
    out << "  skill list|show|call|enable|disable\n";
    out << "  chat-template list|detect|info\n";
    out << "  openai [prompt] [opts]     Send a prompt to a local OpenAI-compatible server\n";
    out << "  config path|show|init      Manage the config file\n";
    out << "  server health [--json]     Check a running local server\n";
    out << "\nInteractive commands: /help /info /history /settings /stats /compact /regenerate "
           "/warmup /tokenize /file /clear-attach /clear /reset /system /model /profile /stop "
           "/temp /top-p /top-k /min-p /typical /n /ctx /export /save /exit\n";
    return out.str();
}

namespace {

std::string json_escape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += static_cast<char>(ch);
                }
        }
    }
    return out;
}

}

void print_config(const Config& cfg, bool as_json) {
    if (as_json) {
        std::cout << "{\n";
        std::cout << "  \"model\":\""
                  << json_escape(cfg.model_path.empty() ? "(auto)" : cfg.model_path.string())
                  << "\",\n";
        std::cout << "  \"model_dir\":\"" << json_escape(cfg.model_dir.string()) << "\",\n";
        std::cout << "  \"config_path\":\"" << json_escape(cfg.config_path.string()) << "\",\n";
        std::cout << "  \"history_path\":\"" << json_escape(cfg.history_path.string()) << "\",\n";
        std::cout << "  \"server_host\":\"" << json_escape(cfg.server_host) << "\",\n";
        std::cout << "  \"server_port\":" << cfg.server_port << ",\n";
        std::cout << "  \"api_key\":" << (cfg.api_key.empty() ? "null" : "\"(set)\"") << ",\n";
        std::cout << "  \"profile\":"
                  << (cfg.profile_name.empty() ? "null"
                                               : "\"" + json_escape(cfg.profile_name) + "\"")
                  << ",\n";
        std::cout << "  \"chat_template\":"
                  << (cfg.chat_template.empty() ? "null"
                                                : "\"" + json_escape(cfg.chat_template) + "\"")
                  << ",\n";
        std::cout << "  \"context_size\":" << cfg.context_size << ",\n";
        std::cout << "  \"batch_size\":" << cfg.batch_size << ",\n";
        std::cout << "  \"threads\":" << (cfg.n_threads == 0 ? 0 : cfg.n_threads) << ",\n";
        std::cout << "  \"gpu_layers\":" << cfg.n_gpu_layers << ",\n";
        std::cout << "  \"temperature\":" << cfg.temperature << ",\n";
        std::cout << "  \"top_p\":" << cfg.top_p << ",\n";
        std::cout << "  \"top_k\":" << cfg.top_k << ",\n";
        std::cout << "  \"min_p\":" << cfg.min_p << ",\n";
        std::cout << "  \"typical_p\":" << cfg.typical_p << ",\n";
        std::cout << "  \"repeat_penalty\":" << cfg.repeat_penalty << ",\n";
        std::cout << "  \"n_predict\":" << cfg.n_predict << ",\n";
        std::cout << "  \"seed\":"
                  << (cfg.seed == 0xFFFFFFFFu ? "\"random\"" : std::to_string(cfg.seed)) << ",\n";
        std::cout << "  \"save_history\":" << (cfg.save_history ? "true" : "false") << ",\n";
        std::cout << "  \"auto_trim\":" << (cfg.auto_trim ? "true" : "false") << ",\n";
        std::cout << "  \"serve\":" << (cfg.serve ? "true" : "false") << ",\n";
        std::cout << "  \"skills_enabled\":" << (cfg.skills_enabled ? "true" : "false") << ",\n";
        std::cout << "  \"enabled_skills\":[";
        for (size_t i = 0; i < cfg.enabled_skills.size(); ++i) {
            if (i > 0) {
                std::cout << ",";
            }
            std::cout << "\"" << json_escape(cfg.enabled_skills[i]) << "\"";
        }
        std::cout << "]\n";
        std::cout << "}\n";
        return;
    }
    std::cout << "model            "
              << (cfg.model_path.empty() ? "(auto)" : cfg.model_path.string()) << "\n";
    std::cout << "model_dir        " << cfg.model_dir.string() << "\n";
    std::cout << "config_path      " << cfg.config_path.string() << "\n";
    std::cout << "history_path     " << cfg.history_path.string() << "\n";
    std::cout << "output_path      "
              << (cfg.output_path.empty() ? "(none)" : cfg.output_path.string()) << "\n";
    std::cout << "export_path      "
              << (cfg.export_path.empty() ? "(none)" : cfg.export_path.string()) << "\n";
    std::cout << "session_name     " << (cfg.session_name.empty() ? "(default)" : cfg.session_name)
              << "\n";
    std::cout << "profile          " << (cfg.profile_name.empty() ? "(none)" : cfg.profile_name)
              << "\n";
    std::cout << "chat_template    "
              << (cfg.chat_template.empty() ? "(model default)" : cfg.chat_template) << "\n";
    std::cout << "server_host      " << cfg.server_host << "\n";
    std::cout << "server_port      " << cfg.server_port << "\n";
    std::cout << "server_api_key   " << (cfg.api_key.empty() ? "(none)" : "(set)") << "\n";
    std::cout << "benchmark_runs   "
              << (cfg.benchmark_runs == 0 ? "(none)" : std::to_string(cfg.benchmark_runs)) << "\n";
    std::cout << "attach_files     ";
    if (cfg.attach_paths.empty()) {
        std::cout << "(none)";
    } else {
        for (size_t i = 0; i < cfg.attach_paths.size(); ++i) {
            if (i != 0) {
                std::cout << ", ";
            }
            std::cout << cfg.attach_paths[i].string();
        }
    }
    std::cout << "\n";
    std::cout << "system_prompt    " << (cfg.system_prompt.empty() ? "(none)" : cfg.system_prompt)
              << "\n";
    std::cout << "context_size     " << cfg.context_size << "\n";
    std::cout << "batch_size       " << cfg.batch_size << "\n";
    std::cout << "ubatch           " << (cfg.n_ubatch == 0 ? "auto" : std::to_string(cfg.n_ubatch))
              << "\n";
    std::cout << "reserve_ctx      " << cfg.reserve_ctx << "\n";
    std::cout << "threads          "
              << (cfg.n_threads == 0 ? "auto" : std::to_string(cfg.n_threads)) << "\n";
    std::cout << "gpu_layers       " << cfg.n_gpu_layers << "\n";
    std::cout << "temperature      " << cfg.temperature << "\n";
    std::cout << "top_p            " << cfg.top_p << "\n";
    std::cout << "top_k            " << cfg.top_k << "\n";
    std::cout << "min_p            " << cfg.min_p << "\n";
    std::cout << "typical_p        " << cfg.typical_p << "\n";
    std::cout << "repeat_penalty   " << cfg.repeat_penalty << "\n";
    std::cout << "repeat_last_n    " << cfg.repeat_last_n << "\n";
    std::cout << "n_predict        " << cfg.n_predict << "\n";
    std::cout << "seed             "
              << (cfg.seed == 0xFFFFFFFFu ? "random" : std::to_string(cfg.seed)) << "\n";
    std::cout << "use_mmap         " << (cfg.use_mmap ? "yes" : "no") << "\n";
    std::cout << "use_mlock        " << (cfg.use_mlock ? "yes" : "no") << "\n";
    std::cout << "flash_attn       " << (cfg.flash_attn ? "yes" : "no") << "\n";
    std::cout << "numa             " << (cfg.numa ? "yes" : "no") << "\n";
    std::cout << "kv_offload       " << (cfg.offload_kqv ? "yes" : "no") << "\n";
    std::cout << "n_keep           " << cfg.n_keep << "\n";
    std::cout << "color            " << (cfg.color ? "yes" : "no") << "\n";
    std::cout << "interactive      " << (cfg.interactive ? "yes" : "no") << "\n";
    std::cout << "save_history     " << (cfg.save_history ? "yes" : "no") << "\n";
    std::cout << "auto_trim        " << (cfg.auto_trim ? "yes" : "no") << "\n";
    std::cout << "json_output      " << (cfg.json_output ? "yes" : "no") << "\n";
    std::cout << "smoke            " << (cfg.smoke ? "yes" : "no") << "\n";
    std::cout << "warmup           " << (cfg.warmup ? "yes" : "no") << "\n";
    std::cout << "serve            " << (cfg.serve ? "yes" : "no") << "\n";
    std::cout << "skills_enabled   " << (cfg.skills_enabled ? "yes" : "no") << "\n";
    std::cout << "enabled_skills   ";
    if (cfg.enabled_skills.empty()) {
        std::cout << "(none)";
    } else {
        for (size_t i = 0; i < cfg.enabled_skills.size(); ++i) {
            if (i != 0) {
                std::cout << ", ";
            }
            std::cout << cfg.enabled_skills[i];
        }
    }
    std::cout << "\n";
    std::cout << "debug            " << (cfg.debug ? "yes" : "no") << "\n";
    std::cout << "context_bar      " << (cfg.context_bar ? "yes" : "no") << "\n";
    std::cout << "backend_info     " << (cfg.backend_info ? "yes" : "no") << "\n";
    std::cout << "stop_sequences   ";
    if (cfg.stop_sequences.empty()) {
        std::cout << "(none)";
    } else {
        for (size_t i = 0; i < cfg.stop_sequences.size(); ++i) {
            if (i != 0) {
                std::cout << ", ";
            }
            std::cout << cfg.stop_sequences[i];
        }
    }
    std::cout << "\n";
}

}
