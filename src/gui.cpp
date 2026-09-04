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

#include <webview/webview.h>

#include <llama.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
};

using AppStatePtr = std::shared_ptr<AppState>;

AppStatePtr g_state;
webview_t g_view = nullptr;
std::string g_cached_model_dir;

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

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
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
    if (state->messages.empty()) {
        return "[]";
    }
    return "[\"Current conversation\"]";
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

void generate_messages(AppStatePtr state, const JsonValue& messages_value,
                       const JsonValue& settings) {
    std::vector<ChatMessage> ask;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (!state->loaded.load() || state->generating.load() || state->loading.load()) {
            return;
        }
        if (!state->system_prompt.empty()) {
            ask.push_back({"system", state->system_prompt});
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

    std::thread([state, ask, gen]() {
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
        GenerationResult result;
        std::string error;
        const bool ok = engine->generate(
            ask, options, result, [state]() { return state->stop_requested.load(); }, error);
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            if (ok && !result.text.empty()) {
                state->messages.push_back({"assistant", result.text});
            }
            state->generating.store(false);
            state->stop_requested.store(false);
            state->status = ok ? "Generation complete" : "Generation interrupted";
        }
        if (ok && !result.text.empty()) {
            std::string event = "{\"type\":\"generated\",\"text\":";
            event += json_escape(result.text);
            event += ",\"stats\":";
            event += json_escape(format_stats(result));
            event += "}";
            post_event(event);
            post_event("{\"type\":\"status\",\"text\":\"Generation complete\",\"kind\":\"ok\"}");
        } else {
            std::string event = "{\"type\":\"error\",\"error\":";
            event += json_escape(error.empty() ? "Generation interrupted" : error);
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
        std::lock_guard<std::mutex> guard(state->mutex);
        result += models_json(state);
        result += ",\"conversations\":";
        result += conversations_json(state);
        result += ",\"version\":";
        result += json_escape("1.6.0");
        result += ",\"backend\":\"WebView2 - llama.cpp\",\"system_prompt\":";
        result += json_escape(state->system_prompt);
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
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "loadConversation") {
        std::lock_guard<std::mutex> guard(state->mutex);
        std::ostringstream out;
        out << "{\"ok\":true,\"messages\":[";
        for (size_t i = 0; i < state->messages.size(); ++i) {
            if (i != 0) {
                out << ",";
            }
            out << "{\"role\":";
            out << json_escape(state->messages[i].role);
            out << ",\"content\":";
            out << json_escape(state->messages[i].content);
            out << "}";
        }
        out << "]}";
        reply(id, out.str());
        return;
    }
    if (method == "deleteConversation") {
        reply(id, "{\"ok\":true}");
        return;
    }
    if (method == "version") {
        reply(id, "{\"version\":\"1.6.0\"}");
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

void discard_log(enum ggml_log_level, const char*, void*) {}

}

int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    llama_backend_init();
    llama_log_set(discard_log, nullptr);

    AppStatePtr state = std::make_shared<AppState>();
    state->cfg = skiffllm::default_config();
    skiffllm::apply_environment(state->cfg);
    {
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata == nullptr || !*appdata) {
            appdata = std::getenv("USERPROFILE");
        }
        if (appdata != nullptr && *appdata) {
            state->cfg.model_dir = std::filesystem::path(appdata) / "SkiffLLM" / "models";
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(state->cfg.model_dir, ec);
    g_cached_model_dir = state->cfg.model_dir.string();
    g_state = state;

    const std::string html = read_resource(kWebResourceId);

    webview_t view = webview_create(0, nullptr);
    g_view = view;
    if (view == nullptr) {
        std::string message =
            "SkiffLLM needs the Microsoft Edge WebView2 Runtime.\n\n"
            "Install it from https://developer.microsoft.com/microsoft-edge/webview2/ and try "
            "again.";
        MessageBoxA(nullptr, message.c_str(), "SkiffLLM", MB_OK | MB_ICONWARNING);
        llama_backend_free();
        CoUninitialize();
        return 1;
    }

    webview_set_title(view, "SkiffLLM");
    webview_set_size(view, 1280, 800, WEBVIEW_HINT_NONE);

    auto* maximize = new MaximizePayload;
    maximize->icon_large = static_cast<HICON>(
        LoadImageW(h_instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                   GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    maximize->icon_small = static_cast<HICON>(
        LoadImageW(h_instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                   GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    webview_dispatch(view, maximize_trampoline, maximize);

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
    CoUninitialize();
    return 0;
}
