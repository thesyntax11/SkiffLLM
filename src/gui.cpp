#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#endif

#include <webview/webview.h>

#include <llama.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "skiffllm/config.hpp"
#include "skiffllm/engine.hpp"
#include "skiffllm/messages.hpp"
#include "skiffllm/session.hpp"
#include "skiffllm/skills.hpp"
#include "skiffllm/tools.hpp"

using skiffllm::ChatMessage;
using skiffllm::Config;
using skiffllm::GenerationOptions;
using skiffllm::GenerationResult;
using skiffllm::SkiffEngine;

namespace {

constexpr int kWebResourceId = 100;

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::pair<std::string, JsonValue>> object;
};

const JsonValue* json_find(const JsonValue& value, const std::string& key) {
    if (value.type != JsonValue::Type::Object) {
        return nullptr;
    }
    for (const auto& entry : value.object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

bool json_bool(const JsonValue& value, bool fallback) {
    return value.type == JsonValue::Type::Bool ? value.boolean : fallback;
}

double json_number(const JsonValue& value, double fallback) {
    return value.type == JsonValue::Type::Number ? value.number : fallback;
}

std::string json_string(const JsonValue& value, const std::string& fallback = "") {
    return value.type == JsonValue::Type::String ? value.string : fallback;
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
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            default:
                if (ch < 0x20) {
                    char buffer[8] = {};
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

class JsonParser {
   public:
    explicit JsonParser(const std::string& input) : text_(input), index_(0) {}

    bool parse(JsonValue& output, std::string& error) {
        skip_space();
        if (!parse_value(output)) {
            error = error_;
            return false;
        }
        skip_space();
        if (index_ != text_.size()) {
            error = "trailing data";
            return false;
        }
        return true;
    }

   private:
    bool parse_value(JsonValue& output) {
        skip_space();
        if (index_ >= text_.size()) {
            error_ = "unexpected end";
            return false;
        }
        const char ch = text_[index_];
        if (ch == '"') {
            output.type = JsonValue::Type::String;
            return parse_string(output.string);
        }
        if (ch == '{') {
            output.type = JsonValue::Type::Object;
            return parse_object(output.object);
        }
        if (ch == '[') {
            output.type = JsonValue::Type::Array;
            return parse_array(output.array);
        }
        if (ch == 't') {
            if (text_.compare(index_, 4, "true") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 4;
            output.type = JsonValue::Type::Bool;
            output.boolean = true;
            return true;
        }
        if (ch == 'f') {
            if (text_.compare(index_, 5, "false") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 5;
            output.type = JsonValue::Type::Bool;
            output.boolean = false;
            return true;
        }
        if (ch == 'n') {
            if (text_.compare(index_, 4, "null") != 0) {
                error_ = "invalid literal";
                return false;
            }
            index_ += 4;
            output.type = JsonValue::Type::Null;
            return true;
        }
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            output.type = JsonValue::Type::Number;
            return parse_number(output.number);
        }
        error_ = "unexpected character";
        return false;
    }

    bool parse_string(std::string& output) {
        ++index_;
        output.clear();
        while (index_ < text_.size()) {
            const char ch = text_[index_++];
            if (ch == '"') {
                return true;
            }
            if (ch != '\\') {
                output.push_back(ch);
                continue;
            }
            if (index_ >= text_.size()) {
                error_ = "bad escape";
                return false;
            }
            const char escape = text_[index_++];
            switch (escape) {
                case '"':
                    output.push_back('"');
                    break;
                case '\\':
                    output.push_back('\\');
                    break;
                case '/':
                    output.push_back('/');
                    break;
                case 'b':
                    output.push_back('\b');
                    break;
                case 'f':
                    output.push_back('\f');
                    break;
                case 'n':
                    output.push_back('\n');
                    break;
                case 'r':
                    output.push_back('\r');
                    break;
                case 't':
                    output.push_back('\t');
                    break;
                case 'u': {
                    if (index_ + 4 > text_.size()) {
                        error_ = "bad unicode";
                        return false;
                    }
                    int value = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char hex = text_[index_++];
                        value <<= 4;
                        if (hex >= '0' && hex <= '9') {
                            value |= hex - '0';
                        } else if (hex >= 'a' && hex <= 'f') {
                            value |= hex - 'a' + 10;
                        } else if (hex >= 'A' && hex <= 'F') {
                            value |= hex - 'A' + 10;
                        } else {
                            error_ = "bad unicode";
                            return false;
                        }
                    }
                    if (value < 0x80) {
                        output.push_back(static_cast<char>(value));
                    } else if (value < 0x800) {
                        output.push_back(static_cast<char>(0xC0 | (value >> 6)));
                        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
                    } else {
                        output.push_back(static_cast<char>(0xE0 | (value >> 12)));
                        output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                        output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
                    }
                    break;
                }
                default:
                    error_ = "bad escape";
                    return false;
            }
        }
        error_ = "unterminated string";
        return false;
    }

    bool parse_number(double& output) {
        const size_t start = index_;
        if (text_[index_] == '-') {
            ++index_;
        }
        while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
            ++index_;
        }
        if (index_ < text_.size() && text_[index_] == '.') {
            ++index_;
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                ++index_;
            }
        }
        if (index_ < text_.size() && (text_[index_] == 'e' || text_[index_] == 'E')) {
            ++index_;
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                ++index_;
            }
            while (index_ < text_.size() && text_[index_] >= '0' && text_[index_] <= '9') {
                ++index_;
            }
        }
        if (index_ == start) {
            error_ = "bad number";
            return false;
        }
        output = std::strtod(text_.substr(start, index_ - start).c_str(), nullptr);
        return true;
    }

    bool parse_object(std::vector<std::pair<std::string, JsonValue>>& output) {
        ++index_;
        skip_space();
        if (index_ < text_.size() && text_[index_] == '}') {
            ++index_;
            return true;
        }
        while (index_ < text_.size()) {
            skip_space();
            if (index_ >= text_.size() || text_[index_] != '"') {
                error_ = "expected key";
                return false;
            }
            std::string key;
            if (!parse_string(key)) {
                return false;
            }
            skip_space();
            if (index_ >= text_.size() || text_[index_] != ':') {
                error_ = "expected colon";
                return false;
            }
            ++index_;
            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            output.emplace_back(std::move(key), std::move(value));
            skip_space();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == '}') {
                ++index_;
                return true;
            }
            error_ = "expected comma";
            return false;
        }
        error_ = "unterminated object";
        return false;
    }

    bool parse_array(std::vector<JsonValue>& output) {
        ++index_;
        skip_space();
        if (index_ < text_.size() && text_[index_] == ']') {
            ++index_;
            return true;
        }
        while (index_ < text_.size()) {
            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            output.push_back(std::move(value));
            skip_space();
            if (index_ < text_.size() && text_[index_] == ',') {
                ++index_;
                continue;
            }
            if (index_ < text_.size() && text_[index_] == ']') {
                ++index_;
                return true;
            }
            error_ = "expected comma";
            return false;
        }
        error_ = "unterminated array";
        return false;
    }

    void skip_space() {
        while (index_ < text_.size()) {
            const char ch = text_[index_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                break;
            }
            ++index_;
        }
    }

    const std::string& text_;
    size_t index_;
    std::string error_;
};

struct AppState {
    std::mutex mutex;
    Config cfg;
    std::shared_ptr<Config> engine_cfg;
    std::shared_ptr<SkiffEngine> engine;
    std::vector<std::filesystem::path> models;
    std::string selected_model;
    std::atomic<bool> loading{false};
    std::atomic<bool> loaded{false};
    std::atomic<bool> generating{false};
    std::atomic<bool> stop_requested{false};
    std::string load_error;
    std::vector<ChatMessage> messages;
    std::string stream;
    std::string status;
    std::string system_prompt;
    std::vector<std::string> enabled_skills;
    bool skills_enabled = false;
    std::string current_conversation = "default";
};

using AppStatePtr = std::shared_ptr<AppState>;

AppStatePtr g_state;
webview_t g_view = nullptr;
std::string g_cached_model_dir;

#ifdef _WIN32
std::string read_resource(int id) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (found == nullptr) {
        return {};
    }
    HGLOBAL loaded = LoadResource(module, found);
    if (loaded == nullptr) {
        return {};
    }
    const DWORD size = SizeofResource(module, found);
    const char* data = static_cast<const char*>(LockResource(loaded));
    if (data == nullptr || size == 0) {
        return {};
    }
    return std::string(data, size);
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size =
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}
#endif

std::string read_ui_html() {
#ifdef _WIN32
    return read_resource(kWebResourceId);
#else
    const char* override_path = std::getenv("SKIFFLLM_WEB_HTML");
    std::vector<std::string> candidates;
    if (override_path != nullptr && *override_path != '\0') {
        candidates.emplace_back(override_path);
    }
    candidates.emplace_back(SKIFFLLM_WEB_HTML);
    std::error_code path_error;
    const auto executable = std::filesystem::canonical("/proc/self/exe", path_error);
    const auto directory = executable.parent_path();
    candidates.push_back((directory / "web" / "index.html").string());
    candidates.push_back(
        (directory.parent_path() / "share" / "skiffllm" / "web" / "index.html").string());
    candidates.push_back((directory.parent_path() / "share" / "skiffllm" / "index.html").string());
    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            continue;
        }
        std::ostringstream out;
        out << file.rdbuf();
        return out.str();
    }
    return {};
#endif
}

std::string models_json(const AppStatePtr& state) {
    std::ostringstream out;
    out << "{\"items\":[";
    for (size_t i = 0; i < state->models.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << json_escape(state->models[i].string());
    }
    out << "],\"dir\":" << json_escape(g_cached_model_dir) << "}";
    return out.str();
}

std::string conversations_json(const AppStatePtr& state) {
    std::string error;
    const auto files = skiffllm::list_session_files(state->cfg, error);
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (const auto& file : files) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << json_escape(file.stem().string());
    }
    if (state->current_conversation != "default" &&
        state->current_conversation != "Current conversation") {
        const std::string candidate = state->current_conversation;
        bool found = false;
        for (const auto& file : files) {
            if (file.stem().string() == candidate) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (!first) {
                out << ",";
            }
            out << json_escape(candidate);
        }
    }
    out << "]";
    return out.str();
}

std::string skills_json(const AppStatePtr& state) {
    std::ostringstream out;
    out << "{\"enabled\":";
    out << (state->skills_enabled ? "true" : "false");
    out << ",\"catalog\":[";
    const auto catalog = skiffllm::skill_catalog();
    bool first = true;
    for (const auto& name : catalog) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << "{\"name\":";
        out << json_escape(name);
        out << ",\"description\":";
        out << json_escape(skiffllm::skill_description(name));
        out << ",\"example\":";
        out << json_escape(skiffllm::skill_example(name));
        out << ",\"enabled\":";
        out << (skiffllm::skill_available(name, state->enabled_skills) ? "true" : "false");
        out << "}";
    }
    out << "]}";
    return out.str();
}

void apply_skill_settings(AppStatePtr state, const JsonValue& params) {
    std::lock_guard<std::mutex> guard(state->mutex);
    const JsonValue* enabled = json_find(params, "enabled");
    const JsonValue* list = json_find(params, "list");
    if (enabled != nullptr) {
        state->skills_enabled = json_bool(*enabled, state->skills_enabled);
    }
    std::vector<std::string> chosen;
    if (list != nullptr && list->type == JsonValue::Type::Array) {
        const auto catalog = skiffllm::skill_catalog();
        for (const auto& item : list->array) {
            if (item.type == JsonValue::Type::String &&
                std::find(catalog.begin(), catalog.end(), item.string) != catalog.end() &&
                std::find(chosen.begin(), chosen.end(), item.string) == chosen.end()) {
                chosen.push_back(item.string);
            }
        }
    }
    state->enabled_skills = chosen;
    if (state->skills_enabled && state->enabled_skills.empty()) {
        state->enabled_skills = skiffllm::skill_catalog();
    }
    state->cfg.skills_enabled = state->skills_enabled;
    state->cfg.enabled_skills = state->enabled_skills;
    std::string save_error;
    skiffllm::write_config_file(state->cfg.config_path, state->cfg, save_error);
}

std::string conversation_name(const std::string& input) {
    std::string name = skiffllm::trim(input);
    if (name.empty()) {
        name = "conversation-" + std::to_string(std::time(nullptr));
    }
    return name;
}

bool save_conversation(AppStatePtr state, const std::string& name, const JsonValue& messages_value,
                       std::string& error) {
    std::lock_guard<std::mutex> guard(state->mutex);
    Config session_cfg = state->cfg;
    session_cfg.history_path = skiffllm::session_file_for(state->cfg, conversation_name(name));
    skiffllm::Session session(session_cfg);
    session.set_system_prompt(state->system_prompt);
    if (messages_value.type == JsonValue::Type::Array) {
        for (const auto& item : messages_value.array) {
            const JsonValue* role = json_find(item, "role");
            const JsonValue* content = json_find(item, "content");
            if (role != nullptr && content != nullptr) {
                session.messages().push_back({json_string(*role), json_string(*content)});
            }
        }
    }
    if (!session.save(error)) {
        return false;
    }
    state->current_conversation = conversation_name(name);
    return true;
}

bool load_conversation(AppStatePtr state, const std::string& name,
                       std::vector<ChatMessage>& messages, std::string& error) {
    std::lock_guard<std::mutex> guard(state->mutex);
    Config session_cfg = state->cfg;
    session_cfg.history_path = skiffllm::session_file_for(state->cfg, name);
    skiffllm::Session session(session_cfg);
    if (!session.load(error)) {
        return false;
    }
    messages = session.messages();
    state->system_prompt = session.system_prompt();
    state->current_conversation = name;
    return true;
}

bool delete_conversation(AppStatePtr state, const std::string& name, std::string& error) {
    std::lock_guard<std::mutex> guard(state->mutex);
    const auto path = skiffllm::session_file_for(state->cfg, name);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return true;
    }
    if (!std::filesystem::remove(path, ec) || ec) {
        error = "cannot remove conversation: " + ec.message();
        return false;
    }
    if (state->current_conversation == name) {
        state->current_conversation = "default";
    }
    return true;
}

void post_event(std::string event) {
    if (g_view == nullptr) {
        return;
    }
    struct Payload {
        std::string script;
    };
    struct OnDispatch {
        static void run(webview_t view, void* raw) {
            auto* payload = static_cast<Payload*>(raw);
            webview_eval(view, payload->script.c_str());
            delete payload;
        }
    };
    auto* payload = new Payload;
    payload->script = "window.__skiffEvent(" + json_escape(event) + ");";
    webview_dispatch(g_view, OnDispatch::run, payload);
}

void reply(const char* id, const std::string& value) {
    if (g_view != nullptr && id != nullptr) {
        webview_return(g_view, id, 0, value.c_str());
    }
}

void reply_error(const char* id, const std::string& message) {
    if (g_view != nullptr && id != nullptr) {
        webview_return(g_view, id, 1, json_escape(message).c_str());
    }
}

void discover_models(AppStatePtr state) {
    std::string error;
    std::vector<std::filesystem::path> found = skiffllm::discover_models(state->cfg, error);
    std::lock_guard<std::mutex> guard(state->mutex);
    state->models = std::move(found);
    if (state->selected_model.empty() && !state->models.empty()) {
        state->selected_model = state->models.front().string();
    }
    if (state->selected_model.empty()) {
        state->status = error.empty() ? "No models found" : error;
    } else {
        state->status = "Select a model";
    }
}

std::string browse_model() {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = reinterpret_cast<HWND>(webview_get_window(g_view));
    ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) {
        return "{\"ok\":false}";
    }
    std::string result;
    result += "{\"ok\":true,\"path\":";
    result += json_escape(wide_to_utf8(path));
    result += "}";
    return result;
#else
    return "{\"ok\":false,\"error\":\"Type the model path in the sidebar\"}";
#endif
}

void load_model_from_path(AppStatePtr state, const std::string& path, const JsonValue& settings) {
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (state->loading.load() || state->loaded.load()) {
            return;
        }
        state->cfg.model_path = skiffllm::expand_path(path);
        if (const JsonValue* value = json_find(settings, "context")) {
            state->cfg.context_size = static_cast<int>(json_number(*value, 4096.0));
        }
        if (const JsonValue* value = json_find(settings, "threads")) {
            state->cfg.n_threads = static_cast<int>(json_number(*value, 8.0));
        }
        if (const JsonValue* value = json_find(settings, "system_prompt")) {
            state->system_prompt = json_string(*value, "");
        }
        state->loading.store(true);
        state->loaded.store(false);
        state->load_error.clear();
        state->status = "Loading model";
    }
    post_event("{\"type\":\"status\",\"text\":\"Loading model\",\"kind\":\"warn\"}");
    std::thread([state, path]() {
        std::shared_ptr<Config> engine_cfg;
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            engine_cfg = std::make_shared<Config>(state->cfg);
        }
        std::shared_ptr<SkiffEngine> engine = std::make_shared<SkiffEngine>(*engine_cfg);
        std::string error;
        const bool ok = engine->load(error);
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            if (ok) {
                state->engine = engine;
                state->engine_cfg = engine_cfg;
                state->loaded.store(true);
                state->status = "Model ready";
            } else {
                state->load_error = error;
                state->status = "Model load failed";
            }
            state->loading.store(false);
        }
        if (ok) {
            post_event("{\"type\":\"loaded\"}");
            post_event("{\"type\":\"status\",\"text\":\"Model ready\",\"kind\":\"ok\"}");
        } else {
            std::string event = "{\"type\":\"status\",\"text\":";
            event += json_escape("Model load failed: " + error) + ",\"kind\":\"warn\"}";
            post_event(event);
        }
    }).detach();
}

void unload_model(AppStatePtr state) {
    std::lock_guard<std::mutex> guard(state->mutex);
    state->stop_requested.store(true);
    state->engine.reset();
    state->engine_cfg.reset();
    state->loaded.store(false);
    state->loading.store(false);
    state->messages.clear();
    state->status = "Model unloaded";
    post_event("{\"type\":\"unloaded\"}");
    post_event("{\"type\":\"status\",\"text\":\"No model loaded\",\"kind\":\"ok\"}");
}

std::string format_stats(const GenerationResult& result) {
    std::ostringstream out;
    out << "Generated " << result.generated_tokens << " tokens in "
        << static_cast<int>(result.generation_ms) << " ms at "
        << static_cast<int>(result.tokens_per_second) << " tok/s";
    return out.str();
}

std::string skill_call_json(const std::string& name,
                            const std::map<std::string, std::string>& args) {
    std::ostringstream out;
    out << "{\"name\":";
    out << json_escape(name);
    out << ",\"args\":{";
    bool first = true;
    for (const auto& entry : args) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << json_escape(entry.first) << ":" << json_escape(entry.second);
    }
    out << "}}";
    return out.str();
}

std::filesystem::path gui_settings_path(const Config& cfg) {
    if (!cfg.config_path.empty() && !cfg.config_path.parent_path().empty()) {
        return cfg.config_path.parent_path() / "skiffllm-gui.json";
    }
    return std::filesystem::path("skiffllm-gui.json");
}

bool save_gui_settings(const AppStatePtr& state, const JsonValue& params, std::string& error) {
    const auto path = gui_settings_path(state->cfg);
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            error = "cannot create settings directory: " + path.parent_path().string();
            return false;
        }
    }
    std::ofstream out(path, std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        error = "cannot open settings file: " + path.string();
        return false;
    }
    const JsonValue* system = json_find(params, "system_prompt");
    const JsonValue* temperature = json_find(params, "temperature");
    const JsonValue* top_p = json_find(params, "top_p");
    const JsonValue* max_tokens = json_find(params, "max_tokens");
    const JsonValue* context = json_find(params, "context");
    const JsonValue* threads = json_find(params, "threads");
    out << "{\"system_prompt\":";
    out << json_escape(system == nullptr ? std::string() : json_string(*system));
    out << ",\"temperature\":" << (temperature == nullptr ? 0.7 : json_number(*temperature, 0.7));
    out << ",\"top_p\":" << (top_p == nullptr ? 0.95 : json_number(*top_p, 0.95));
    out << ",\"max_tokens\":" << (max_tokens == nullptr ? 512 : json_number(*max_tokens, 512.0));
    out << ",\"context\":" << (context == nullptr ? 4096 : json_number(*context, 4096.0));
    out << ",\"threads\":" << (threads == nullptr ? 0 : json_number(*threads, 0.0));
    out << "}\n";
    return true;
}

std::string load_gui_settings(const AppStatePtr& state) {
    const auto path = gui_settings_path(state->cfg);
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return "{}";
    }
    std::string content;
    char buffer[4096];
    while (input.read(buffer, sizeof(buffer))) {
        content.append(buffer, input.gcount());
    }
    content.append(buffer, input.gcount());
    const std::string trimmed = skiffllm::trim(content);
    if (trimmed.size() >= 2 && trimmed.front() == '{' && trimmed.back() == '}') {
        return trimmed;
    }
    return "{}";
}

void generate_messages(AppStatePtr state, const JsonValue& messages_value,
                       const JsonValue& settings) {
    std::vector<ChatMessage> ask;
    bool skills_enabled = false;
    std::vector<std::string> enabled_skills;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (!state->loaded.load() || state->generating.load() || state->loading.load()) {
            return;
        }
        if (state->skills_enabled && !state->enabled_skills.empty()) {
            skills_enabled = true;
            enabled_skills = state->enabled_skills;
        }
        std::string system = state->system_prompt;
        if (skills_enabled) {
            const std::string instructions = skiffllm::skill_instructions(enabled_skills);
            system = system.empty() ? instructions : system + "\n\n" + instructions;
        }
        if (!system.empty()) {
            ask.push_back({"system", system});
        }
        for (const auto& item : messages_value.array) {
            const JsonValue* role = json_find(item, "role");
            const JsonValue* content = json_find(item, "content");
            if (role != nullptr && content != nullptr) {
                ask.push_back({json_string(*role), json_string(*content)});
            }
        }
        if (ask.empty()) {
            return;
        }
        state->generating.store(true);
        state->stop_requested.store(false);
        state->stream.clear();
        state->status = "Generating";
    }
    post_event("{\"type\":\"status\",\"text\":\"Generating\",\"kind\":\"warn\"}");

    struct GenerationSettings {
        float temperature = 0.7f;
        float top_p = 0.95f;
        int max_tokens = 512;
    };

    GenerationSettings gen;
    if (const JsonValue* value = json_find(settings, "temperature")) {
        gen.temperature = static_cast<float>(json_number(*value, 0.7));
    }
    if (const JsonValue* value = json_find(settings, "top_p")) {
        gen.top_p = static_cast<float>(json_number(*value, 0.95));
    }
    if (const JsonValue* value = json_find(settings, "max_tokens")) {
        gen.max_tokens = static_cast<int>(json_number(*value, 512.0));
    }

    std::thread([state, ask, gen, skills_enabled, enabled_skills]() {
        std::shared_ptr<Config> cfg;
        std::shared_ptr<SkiffEngine> engine;
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            cfg = state->engine_cfg;
            engine = state->engine;
        }
        if (!engine || !cfg) {
            std::lock_guard<std::mutex> guard(state->mutex);
            state->generating.store(false);
            post_event("{\"type\":\"error\",\"error\":\"Engine is unavailable\"}");
            return;
        }
        GenerationOptions options;
        options.n_predict = gen.max_tokens;
        options.temperature = gen.temperature;
        options.top_p = gen.top_p;
        options.min_p = cfg->min_p;
        options.typical_p = cfg->typical_p;
        options.top_k = cfg->top_k;
        options.repeat_penalty = cfg->repeat_penalty;
        options.repeat_last_n = cfg->repeat_last_n;
        options.seed = cfg->seed;
        options.auto_trim = cfg->auto_trim;
        options.reserve_ctx = cfg->reserve_ctx;
        options.n_keep = cfg->n_keep;
        options.stop_sequences = cfg->stop_sequences;

        std::vector<ChatMessage> conversation = ask;
        std::string final_text;
        GenerationResult final_result;
        bool ok = true;
        std::string last_error;
        bool used_skills = false;

        for (int round = 0; round < 10; ++round) {
            {
                std::lock_guard<std::mutex> guard(state->mutex);
                state->stream.clear();
            }
            options.token_callback = [state](const std::string& part) {
                std::string stream;
                {
                    std::lock_guard<std::mutex> guard(state->mutex);
                    state->stream += part;
                    stream = state->stream;
                }
                std::string event = "{\"type\":\"token\",\"text\":";
                event += json_escape(stream);
                event += "}";
                post_event(event);
            };
            const bool generated = engine->generate(
                conversation, options, final_result,
                [state]() { return state->stop_requested.load(); }, last_error);
            if (!generated) {
                ok = false;
                break;
            }
            final_text = final_result.text;
            if (!skills_enabled) {
                break;
            }
            std::vector<skiffllm::SkillRequest> requests;
            std::string skill_error;
            if (!skiffllm::parse_skill_requests(final_text, requests, skill_error) ||
                requests.empty()) {
                break;
            }
            conversation.push_back({"assistant", final_text});
            bool executed = false;
            for (const auto& request : requests) {
                std::string result;
                std::string error;
                if (!skiffllm::skill_available(request.name, enabled_skills)) {
                    result = "Error: skill is not enabled";
                    error = "skill is not enabled";
                } else {
                    result = skiffllm::execute_skill(*cfg, request, error);
                }
                used_skills = true;
                std::string event = "{\"type\":\"skill\",\"call\":";
                event += skill_call_json(request.name, request.args);
                event += ",\"result\":";
                event += json_escape(error.empty() ? result : "Error: " + error);
                event += ",\"error\":";
                event += json_escape(error);
                event += "}";
                post_event(event);
                conversation.push_back({"user", "Skill result for " + request.name + ":\n" +
                                                    (error.empty() ? result : "Error: " + error)});
                executed = true;
            }
            if (!executed) {
                break;
            }
        }

        if (ok && used_skills) {
            final_text = skiffllm::strip_skill_markers(final_text);
        }
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            if (ok && !final_text.empty()) {
                state->messages.push_back({"assistant", final_text});
            }
            state->generating.store(false);
            state->stop_requested.store(false);
            state->status = ok ? "Generation complete" : "Generation interrupted";
        }
        if (ok && !final_text.empty()) {
            std::string event = "{\"type\":\"generated\",\"text\":";
            event += json_escape(final_text);
            event += ",\"stats\":";
            event += json_escape(format_stats(final_result));
            event += "}";
            post_event(event);
            post_event("{\"type\":\"status\",\"text\":\"Generation complete\",\"kind\":\"ok\"}");
        } else {
            std::string event = "{\"type\":\"error\",\"error\":";
            event += json_escape(last_error.empty() ? "Generation interrupted" : last_error);
            event += "}";
            post_event(event);
        }
    }).detach();
}

void handle_skiff(const char* id, const std::string& method, const JsonValue& params) {
    AppStatePtr state = g_state;
    if (method == "init") {
        discover_models(state);
        std::string result;
        result += "{\"models\":";
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            result += models_json(state);
            result += ",\"conversations\":";
            result += conversations_json(state);
            result += ",\"skills\":";
            result += skills_json(state);
            result += ",\"version\":";
            result += json_escape(SKIFFLLM_VERSION);
            result += ",\"backend\":";
            result += json_escape(desktop_backend_name());
            result += ",\"system_prompt\":";
            result += json_escape(state->system_prompt);
        }
        result += "}";
        reply(id, result);
        return;
    }
    if (method == "listModels") {
        discover_models(state);
        std::lock_guard<std::mutex> guard(state->mutex);
        reply(id, models_json(state));
        return;
    }
    if (method == "browseModel") {
        reply(id, browse_model());
        return;
    }
    if (method == "loadModel") {
        const JsonValue* path = json_find(params, "path");
        const JsonValue* settings = json_find(params, "settings");
        if (path == nullptr || json_string(*path).empty()) {
            reply_error(id, "No model path provided");
            return;
        }
        const JsonValue empty_settings;
        load_model_from_path(state, json_string(*path),
                             settings == nullptr ? empty_settings : *settings);
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "unloadModel") {
        unload_model(state);
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "generate") {
        const JsonValue* messages = json_find(params, "messages");
        const JsonValue* settings = json_find(params, "settings");
        if (messages == nullptr || messages->type != JsonValue::Type::Array) {
            reply_error(id, "No messages provided");
            return;
        }
        const JsonValue empty_settings;
        generate_messages(state, *messages, settings == nullptr ? empty_settings : *settings);
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "stop") {
        state->stop_requested.store(true);
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "newConversation") {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->messages.clear();
        state->current_conversation = "default";
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "loadConversation") {
        const JsonValue* name = json_find(params, "name");
        if (name == nullptr || json_string(*name).empty()) {
            reply_error(id, "No conversation name provided");
            return;
        }
        std::vector<ChatMessage> loaded;
        std::string error;
        if (!load_conversation(state, json_string(*name), loaded, error)) {
            reply_error(id, error);
            return;
        }
        std::ostringstream out;
        out << "{\"ok\":true,\"messages\":[";
        for (size_t i = 0; i < loaded.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << "{\"role\":";
            out << json_escape(loaded[i].role);
            out << ",\"content\":";
            out << json_escape(loaded[i].content);
            out << "}";
        }
        out << "],\"system_prompt\":";
        out << json_escape(state->system_prompt);
        out << "}";
        reply(id, out.str());
        return;
    }
    if (method == "saveConversation") {
        const JsonValue* name = json_find(params, "name");
        const JsonValue* messages = json_find(params, "messages");
        if (name == nullptr || messages == nullptr || messages->type != JsonValue::Type::Array) {
            reply_error(id, "saveConversation requires name and messages");
            return;
        }
        std::string error;
        if (!save_conversation(state, json_string(*name), *messages, error)) {
            reply_error(id, error);
            return;
        }
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "listConversations") {
        std::lock_guard<std::mutex> guard(state->mutex);
        std::ostringstream out;
        out << "{\"conversations\":";
        out << conversations_json(state);
        out << "}";
        reply(id, out.str());
        return;
    }
    if (method == "deleteConversation") {
        const JsonValue* name = json_find(params, "name");
        if (name == nullptr || json_string(*name).empty()) {
            reply_error(id, "No conversation name provided");
            return;
        }
        std::string error;
        if (!delete_conversation(state, json_string(*name), error)) {
            reply_error(id, error);
            return;
        }
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "skills") {
        std::lock_guard<std::mutex> guard(state->mutex);
        reply(id, skills_json(state));
        return;
    }
    if (method == "setSkills") {
        apply_skill_settings(state, params);
        std::lock_guard<std::mutex> guard(state->mutex);
        reply(id, skills_json(state));
        return;
    }
    if (method == "executeSkill") {
        const JsonValue* name = json_find(params, "name");
        if (name == nullptr || json_string(*name).empty()) {
            reply_error(id, "No skill name provided");
            return;
        }
        skiffllm::SkillRequest request;
        request.name = json_string(*name);
        if (const JsonValue* args = json_find(params, "args")) {
            if (args->type == JsonValue::Type::Object) {
                for (const auto& entry : args->object) {
                    request.args[entry.first] =
                        entry.second.type == JsonValue::Type::String
                            ? entry.second.string
                            : (entry.second.type == JsonValue::Type::Number
                                   ? std::to_string(entry.second.number)
                                   : (entry.second.type == JsonValue::Type::Bool
                                          ? (entry.second.boolean ? "true" : "false")
                                          : ""));
                }
            }
        }
        std::string error;
        const std::string result = skiffllm::execute_skill(state->cfg, request, error);
        std::ostringstream out;
        out << "{\"ok\":";
        out << (error.empty() ? "true" : "false");
        out << ",\"result\":";
        out << json_escape(result);
        out << ",\"error\":";
        out << json_escape(error);
        out << "}";
        reply(id, out.str());
        return;
    }
    if (method == "modelInfo") {
        std::lock_guard<std::mutex> guard(state->mutex);
        std::ostringstream out;
        if (!state->engine || !state->loaded.load()) {
            out << "{\"loaded\":false}";
            reply(id, out.str());
            return;
        }
        const auto& info = state->engine->info();
        out << "{\"loaded\":true,\"path\":";
        out << json_escape(state->cfg.model_path.string());
        out << ",\"description\":";
        out << json_escape(info.description);
        out << ",\"file_type\":";
        out << json_escape(info.file_type);
        out << ",\"params\":" << info.n_params;
        out << ",\"size_bytes\":" << info.size_bytes;
        out << ",\"context_capacity\":" << state->engine->context_capacity();
        out << ",\"context\":" << state->cfg.context_size;
        out << ",\"threads\":" << state->cfg.n_threads;
        out << ",\"backend\":";
        out << json_escape(state->engine->active_backends());
        out << "}";
        reply(id, out.str());
        return;
    }
    if (method == "getMemories") {
        std::lock_guard<std::mutex> guard(state->mutex);
        std::string out = "{\"memories\":";
        out += json_escape(skiffllm::load_memories(state->cfg));
        out += "}";
        reply(id, out);
        return;
    }
    if (method == "saveMemory") {
        const JsonValue* text = json_find(params, "text");
        if (text == nullptr || json_string(*text).empty()) {
            reply_error(id, "No memory text provided");
            return;
        }
        std::string error;
        if (!skiffllm::append_memory(state->cfg, json_string(*text), error)) {
            reply_error(id, error);
            return;
        }
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "clearMemories") {
        std::string error;
        if (!skiffllm::clear_memories(state->cfg, error)) {
            reply_error(id, error);
            return;
        }
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "getStats") {
        skiffllm::UsageStats stats;
        std::string error;
        const bool ok = skiffllm::load_usage_stats(state->cfg, stats, error);
        std::ostringstream out;
        out << "{\"ok\":" << (ok ? "true" : "false");
        out << ",\"sessions\":" << stats.sessions;
        out << ",\"messages\":" << stats.messages;
        out << ",\"prompt_tokens\":" << stats.prompt_tokens;
        out << ",\"generated_tokens\":" << stats.generated_tokens;
        out << ",\"total_prompt_ms\":" << stats.total_prompt_ms;
        out << ",\"total_generation_ms\":" << stats.total_generation_ms;
        out << ",\"error\":" << json_escape(error);
        out << "}";
        reply(id, out.str());
        return;
    }
    if (method == "saveSettings") {
        std::string error;
        if (!save_gui_settings(state, params, error)) {
            reply_error(id, error);
            return;
        }
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "loadSettings") {
        reply(id, load_gui_settings(state));
        return;
    }
    if (method == "version") {
        reply(id, std::string("{\"version\":") + json_escape(SKIFFLLM_VERSION) + "}");
        return;
    }
    reply_error(id, "Unknown method");
}

void skiff_callback(const char* id, const char* req, void*) {
    if (id == nullptr) {
        return;
    }
    const std::string request = req == nullptr ? "[]" : req;
    JsonValue root;
    std::string error;
    JsonParser request_parser(request);
    if (!request_parser.parse(root, error) || root.type != JsonValue::Type::Array ||
        root.array.empty() || root.array[0].type != JsonValue::Type::String) {
        reply_error(id, "Invalid request");
        return;
    }
    const std::string method = root.array[0].string;
    JsonValue params;
    if (root.array.size() > 1) {
        if (root.array[1].type == JsonValue::Type::String) {
            JsonParser params_parser(root.array[1].string);
            params_parser.parse(params, error);
        } else {
            params = root.array[1];
        }
    }
    handle_skiff(id, method, params);
}

#ifdef _WIN32
struct MaximizePayload {
    HICON icon_large;
    HICON icon_small;
};

void maximize_trampoline(webview_t view, void* raw) {
    auto* payload = static_cast<MaximizePayload*>(raw);
    HWND window = reinterpret_cast<HWND>(webview_get_window(view));
    if (window != nullptr) {
        if (payload->icon_large != nullptr) {
            SendMessageW(window, WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(payload->icon_large));
        }
        if (payload->icon_small != nullptr) {
            SendMessageW(window, WM_SETICON, ICON_SMALL,
                         reinterpret_cast<LPARAM>(payload->icon_small));
        }
        ShowWindow(window, SW_MAXIMIZE);
    }
    delete payload;
}
#endif

void discard_log(enum ggml_log_level, const char*, void*) {}

std::string desktop_backend_name() {
#ifdef _WIN32
    return "WebView2 - llama.cpp";
#elif defined(__APPLE__)
    return "WebKit - llama.cpp";
#else
    return "WebKitGTK - llama.cpp";
#endif
}

int run_app() {
    llama_backend_init();
    llama_log_set(discard_log, nullptr);

    AppStatePtr state = std::make_shared<AppState>();
    state->cfg = skiffllm::default_config();
    skiffllm::apply_environment(state->cfg);
    std::string config_error;
    if (!state->cfg.config_path.empty() && std::filesystem::exists(state->cfg.config_path)) {
        skiffllm::parse_config_file(state->cfg.config_path, state->cfg, config_error);
    }
    state->skills_enabled = state->cfg.skills_enabled;
    state->enabled_skills = state->cfg.enabled_skills;
    std::error_code ec;
    std::filesystem::create_directories(state->cfg.model_dir, ec);
    if (!state->cfg.history_path.empty()) {
        std::filesystem::create_directories(state->cfg.history_path.parent_path(), ec);
    }
    g_cached_model_dir = state->cfg.model_dir.string();
    g_state = state;

    const std::string html = read_ui_html();

    webview_t view = webview_create(0, nullptr);
    g_view = view;
    if (view == nullptr) {
        llama_backend_free();
        return 1;
    }

    webview_set_title(view, "SkiffLLM");
    webview_set_size(view, 1280, 800, WEBVIEW_HINT_NONE);

#ifdef _WIN32
    HINSTANCE instance = GetModuleHandleW(nullptr);
    auto* maximize = new MaximizePayload;
    maximize->icon_large = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                   GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    maximize->icon_small = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                   GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    webview_dispatch(view, maximize_trampoline, maximize);
#endif

    webview_bind(view, "skiff", skiff_callback, nullptr);
    if (!html.empty()) {
        webview_set_html(view, html.c_str());
    } else {
        webview_navigate(view, "data:text/html,%3Ch1%3ESkiffLLM%20UI%20missing%3C%2Fh1%3E");
    }

    webview_run(view);

    state->stop_requested.store(true);
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->engine.reset();
        state->engine_cfg.reset();
    }
    webview_destroy(view);
    g_view = nullptr;
    llama_backend_free();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const int result = run_app();
    CoUninitialize();
    return result;
}
#else
int main(int, char**) {
    return run_app();
}
#endif
