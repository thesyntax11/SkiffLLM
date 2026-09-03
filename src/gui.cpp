#include <commctrl.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
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
using skiffllm::ModelInfo;
using skiffllm::SkiffEngine;

namespace {

constexpr UINT kModelMessage = WM_APP + 1;
constexpr UINT kGenerationDone = WM_APP + 2;
constexpr UINT kUpdateUi = WM_APP + 3;
constexpr UINT kRefreshTimer = 1;

constexpr int kCtlTitle = 1001;
constexpr int kCtlModelDir = 1002;
constexpr int kCtlModels = 1003;
constexpr int kCtlRefresh = 1004;
constexpr int kCtlLoad = 1005;
constexpr int kCtlUnload = 1006;
constexpr int kCtlBrowse = 1007;
constexpr int kCtlModelPath = 1008;
constexpr int kCtlSettings = 1009;
constexpr int kCtlTemp = 1010;
constexpr int kCtlTempVal = 1011;
constexpr int kCtlTopP = 1012;
constexpr int kCtlTopPVal = 1013;
constexpr int kCtlTokens = 1014;
constexpr int kCtlTokensVal = 1015;
constexpr int kCtlContext = 1016;
constexpr int kCtlContextVal = 1017;
constexpr int kCtlThreads = 1018;
constexpr int kCtlThreadsVal = 1019;
constexpr int kCtlChat = 1020;
constexpr int kCtlInput = 1021;
constexpr int kCtlSend = 1022;
constexpr int kCtlStop = 1023;
constexpr int kCtlClear = 1024;
constexpr int kCtlHelp = 1025;
constexpr int kCtlExit = 1026;
constexpr int kCtlStatus = 1027;

struct GuiState {
    std::mutex mutex;
    Config cfg;
    std::shared_ptr<Config> engine_cfg;
    std::shared_ptr<SkiffEngine> engine;
    std::vector<std::filesystem::path> models;
    int selected_model = 0;
    std::string custom_model;
    std::atomic<bool> loading{false};
    std::atomic<bool> loaded{false};
    std::atomic<bool> generating{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> dirty{false};
    std::string load_error;
    std::vector<ChatMessage> messages;
    std::string stream;
    std::string shown_stream;
    std::string worker_error;
    std::string status;
};

using GuiStatePtr = std::shared_ptr<GuiState>;

GuiStatePtr g_state;
std::string state_cached_model_dir;
HWND g_window = nullptr;
HFONT g_font = nullptr;
HFONT g_title_font = nullptr;
HBRUSH g_background = nullptr;
HBRUSH g_panel = nullptr;
HBRUSH g_edit_background = nullptr;
COLORREF g_text_color = RGB(230, 236, 245);
COLORREF g_muted_color = RGB(148, 160, 180);
COLORREF g_accent_color = RGB(110, 184, 255);
COLORREF g_panel_color = RGB(24, 30, 44);
COLORREF g_background_color = RGB(14, 18, 28);
COLORREF g_edit_color = RGB(20, 25, 38);

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

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::wstring format_value(const char* label, int value) {
    std::wstring text = utf8_to_wide(label);
    text += L" ";
    text += std::to_wstring(value);
    return text;
}

std::wstring format_slider_value(const char* label, int value, int scale) {
    std::wstring text = utf8_to_wide(label);
    text += L" ";
    if (scale == 100) {
        wchar_t buffer[32] = {};
        swprintf(buffer, 32, L"%.2f", static_cast<float>(value) / 100.0f);
        text += buffer;
    } else {
        text += std::to_wstring(value);
    }
    return text;
}

void set_text(HWND control, const std::wstring& text) {
    SetWindowTextW(control, text.c_str());
}

std::wstring get_text(HWND control) {
    const int len = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(len), L'\0');
    GetWindowTextW(control, text.data(), len + 1);
    return text;
}

void append_chat(const std::string& text) {
    if (text.empty()) {
        return;
    }
    HWND chat = GetDlgItem(g_window, kCtlChat);
    const std::wstring wide = utf8_to_wide(text);
    SendMessageW(chat, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(chat, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wide.c_str()));
}

void append_chat_line(const std::string& label, const std::string& text) {
    std::string combined;
    combined += "[" + label + "] " + text + "\r\n";
    append_chat(combined);
}

void clear_chat_display() {
    HWND chat = GetDlgItem(g_window, kCtlChat);
    SetWindowTextW(chat, L"");
}

void refresh_models(GuiStatePtr state) {
    std::string error;
    std::vector<std::filesystem::path> models = skiffllm::discover_models(state->cfg, error);
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->models = std::move(models);
        state->selected_model = 0;
        state->status =
            error.empty() ? (state->models.empty() ? "No models found" : "Select a model") : error;
        state->dirty.store(true);
    }
    HWND list = GetDlgItem(g_window, kCtlModels);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& model : models) {
        const std::wstring name = utf8_to_wide(model.filename().string());
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }
    if (!models.empty()) {
        SendMessageW(list, LB_SETCURSEL, 0, 0);
    }
    PostMessageW(g_window, kUpdateUi, 0, 0);
}

void stop_generation(GuiStatePtr state) {
    state->stop_requested.store(true);
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->status = "Stopping generation...";
        state->dirty.store(true);
    }
    PostMessageW(g_window, kUpdateUi, 0, 0);
}

void start_load(GuiStatePtr state) {
    std::string model_path;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (state->loading.load() || state->loaded.load()) {
            return;
        }
        state->cfg.n_predict =
            static_cast<int>(SendMessageW(GetDlgItem(g_window, kCtlTokens), TBM_GETPOS, 0, 0));
        state->cfg.context_size =
            static_cast<int>(SendMessageW(GetDlgItem(g_window, kCtlContext), TBM_GETPOS, 0, 0));
        state->cfg.temperature =
            static_cast<float>(SendMessageW(GetDlgItem(g_window, kCtlTemp), TBM_GETPOS, 0, 0)) /
            100.0f;
        state->cfg.top_p =
            static_cast<float>(SendMessageW(GetDlgItem(g_window, kCtlTopP), TBM_GETPOS, 0, 0)) /
            100.0f;
        state->cfg.n_threads =
            static_cast<int>(SendMessageW(GetDlgItem(g_window, kCtlThreads), TBM_GETPOS, 0, 0));
        if (state->selected_model >= 0 &&
            state->selected_model < static_cast<int>(state->models.size())) {
            model_path = state->models[static_cast<size_t>(state->selected_model)].string();
        }
        const std::string custom = wide_to_utf8(get_text(GetDlgItem(g_window, kCtlModelPath)));
        if (!custom.empty()) {
            model_path = skiffllm::expand_path(custom).string();
        }
        if (model_path.empty()) {
            state->load_error = "Choose a model first.";
            state->status = "Model load failed";
            state->dirty.store(true);
            return;
        }
        state->cfg.model_path = model_path;
        state->loading.store(true);
        state->loaded.store(false);
        state->load_error.clear();
        state->status = "Loading model...";
        state->dirty.store(true);
    }
    GuiStatePtr shared = state;
    std::thread([shared]() {
        std::shared_ptr<Config> engine_cfg;
        {
            std::lock_guard<std::mutex> guard(shared->mutex);
            engine_cfg = std::make_shared<Config>(shared->cfg);
        }
        std::shared_ptr<SkiffEngine> engine = std::make_shared<SkiffEngine>(*engine_cfg);
        std::string error;
        const bool ok = engine->load(error);
        {
            std::lock_guard<std::mutex> guard(shared->mutex);
            if (ok) {
                shared->engine = engine;
                shared->engine_cfg = engine_cfg;
                shared->loaded.store(true);
                shared->status = "Model ready";
            } else {
                shared->load_error = error;
                shared->status = "Model load failed";
            }
            shared->loading.store(false);
            shared->dirty.store(true);
        }
        PostMessageW(g_window, kModelMessage, ok ? 1 : 0, 0);
    }).detach();
}

void unload_model(GuiStatePtr state) {
    std::lock_guard<std::mutex> guard(state->mutex);
    state->engine.reset();
    state->engine_cfg.reset();
    state->loaded.store(false);
    state->loading.store(false);
    state->status = "Model unloaded";
    state->stream.clear();
    state->worker_error.clear();
    state->dirty.store(true);
    PostMessageW(g_window, kUpdateUi, 0, 0);
}

void start_generate(GuiStatePtr state) {
    std::vector<ChatMessage> ask;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (!state->loaded.load() || state->generating.load() || state->loading.load()) {
            return;
        }
        const std::string input = wide_to_utf8(get_text(GetDlgItem(g_window, kCtlInput)));
        const std::string trimmed = skiffllm::trim(input);
        if (trimmed.empty()) {
            return;
        }
        state->messages.push_back({"user", input});
        ask = state->messages;
        state->stream.clear();
        state->shown_stream.clear();
        state->worker_error.clear();
        state->generating.store(true);
        state->stop_requested.store(false);
        state->status = "Generating...";
        state->dirty.store(true);
    }
    append_chat_line("You", ask.back().content);
    SetWindowTextW(GetDlgItem(g_window, kCtlInput), L"");
    GuiStatePtr shared = state;
    std::thread([shared, ask]() {
        std::shared_ptr<Config> cfg;
        std::shared_ptr<SkiffEngine> engine;
        {
            std::lock_guard<std::mutex> guard(shared->mutex);
            cfg = shared->engine_cfg;
            engine = shared->engine;
        }
        if (!engine || !cfg) {
            std::lock_guard<std::mutex> guard(shared->mutex);
            shared->worker_error = "Engine is unavailable.";
            shared->generating.store(false);
            shared->status = "Generation failed";
            shared->dirty.store(true);
            return;
        }
        GenerationOptions options;
        options.n_predict = cfg->n_predict;
        options.temperature = cfg->temperature;
        options.top_p = cfg->top_p;
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
        options.token_callback = [shared](const std::string& part) {
            {
                std::lock_guard<std::mutex> guard(shared->mutex);
                shared->stream += part;
            }
            if (!shared->dirty.exchange(true)) {
                PostMessageW(g_window, kUpdateUi, 0, 0);
            }
        };
        GenerationResult result;
        std::string error;
        const bool ok = engine->generate(
            ask, options, result, [shared]() { return shared->stop_requested.load(); }, error);
        {
            std::lock_guard<std::mutex> guard(shared->mutex);
            if (ok && !result.text.empty()) {
                shared->messages.push_back({"assistant", result.text});
            } else if (!error.empty()) {
                shared->worker_error = error;
            }
            shared->generating.store(false);
            shared->stop_requested.store(false);
            shared->status = ok ? "Generation complete" : "Generation interrupted";
            shared->dirty.store(true);
        }
        PostMessageW(g_window, kGenerationDone, ok ? 1 : 0, 0);
    }).detach();
}

void clear_chat(GuiStatePtr state) {
    std::lock_guard<std::mutex> guard(state->mutex);
    state->messages.clear();
    state->stream.clear();
    state->shown_stream.clear();
    state->worker_error.clear();
    state->status = "Conversation cleared";
    state->dirty.store(true);
    clear_chat_display();
}

void set_status_text(const std::string& text) {
    set_text(GetDlgItem(g_window, kCtlStatus), utf8_to_wide(text));
}

void update_status_from_state(GuiStatePtr state) {
    std::string status;
    std::string error;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        status = state->status;
        error = state->load_error;
    }
    std::string combined = status;
    if (!error.empty()) {
        combined += " - " + error;
    }
    set_status_text(combined);
}

void update_generation_view(GuiStatePtr state) {
    std::string stream;
    std::string shown;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        stream = state->stream;
        shown = state->shown_stream;
    }
    if (shown != stream) {
        const size_t prefix = shown.size();
        if (prefix < stream.size()) {
            append_chat(stream.substr(prefix));
        }
        {
            std::lock_guard<std::mutex> guard(state->mutex);
            state->shown_stream = stream;
        }
    }
}

void create_controls(HWND hwnd) {
    CreateWindowExW(0, L"STATIC", L"SkiffLLM", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlTitle), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlModelDir), nullptr, nullptr);
    CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
                    0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kCtlModels), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlRefresh), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Load Model", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlLoad), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Unload", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlUnload), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlBrowse), nullptr, nullptr);
    CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlModelPath), nullptr, nullptr);

    CreateWindowExW(0, L"STATIC", L"Generation Settings", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0,
                    0, hwnd, reinterpret_cast<HMENU>(kCtlSettings), nullptr, nullptr);

    const int slider_ids[] = {kCtlTemp, kCtlTopP, kCtlTokens, kCtlContext, kCtlThreads};
    const int value_ids[] = {kCtlTempVal, kCtlTopPVal, kCtlTokensVal, kCtlContextVal,
                             kCtlThreadsVal};
    for (int i = 0; i < 5; ++i) {
        CreateWindowExW(0, L"msctls_trackbar32", L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, 0, 0,
                        0, 0, hwnd, reinterpret_cast<HMENU>(slider_ids[i]), nullptr, nullptr);
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hwnd,
                        reinterpret_cast<HMENU>(value_ids[i]), nullptr, nullptr);
    }

    CreateWindowExW(0, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0,
                    0, 0, 0, hwnd, reinterpret_cast<HMENU>(kCtlChat), nullptr, nullptr);
    CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0,
                    hwnd, reinterpret_cast<HMENU>(kCtlInput), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlSend), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlStop), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlClear), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Help", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlHelp), nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlExit), nullptr, nullptr);
    CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
                    reinterpret_cast<HMENU>(kCtlStatus), nullptr, nullptr);

    {
        const int ranges[][3] = {
            {kCtlTemp, 0, 200},        {kCtlTopP, 0, 100},   {kCtlTokens, 16, 4096},
            {kCtlContext, 512, 32768}, {kCtlThreads, 1, 64},
        };
        for (const auto& range : ranges) {
            SendMessageW(GetDlgItem(hwnd, range[0]), TBM_SETRANGE, TRUE,
                         MAKELPARAM(range[1], range[2]));
            SendMessageW(GetDlgItem(hwnd, range[0]), TBM_SETPAGESIZE, 0, 1);
        }
        SendMessageW(GetDlgItem(hwnd, kCtlTemp), TBM_SETPOS, TRUE, 70);
        SendMessageW(GetDlgItem(hwnd, kCtlTopP), TBM_SETPOS, TRUE, 95);
        SendMessageW(GetDlgItem(hwnd, kCtlTokens), TBM_SETPOS, TRUE, 512);
        SendMessageW(GetDlgItem(hwnd, kCtlContext), TBM_SETPOS, TRUE, 4096);
        SendMessageW(GetDlgItem(hwnd, kCtlThreads), TBM_SETPOS, TRUE, 8);
    }
}

void layout_controls(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int sidebar = std::max(320, width / 4);
    const int margin = 12;
    int y = margin;

    HWND title = GetDlgItem(hwnd, kCtlTitle);
    SetWindowPos(title, nullptr, margin, y, sidebar - 2 * margin, 32, SWP_NOZORDER);
    y += 36;

    HWND dir = GetDlgItem(hwnd, kCtlModelDir);
    SetWindowPos(dir, nullptr, margin, y, sidebar - 2 * margin, 22, SWP_NOZORDER);
    y += 26;

    HWND models = GetDlgItem(hwnd, kCtlModels);
    SetWindowPos(models, nullptr, margin, y, sidebar - 2 * margin, 120, SWP_NOZORDER);
    y += 126;

    HWND browse = GetDlgItem(hwnd, kCtlBrowse);
    HWND refresh = GetDlgItem(hwnd, kCtlRefresh);
    HWND load = GetDlgItem(hwnd, kCtlLoad);
    HWND unload = GetDlgItem(hwnd, kCtlUnload);
    const int btn_w = (sidebar - 2 * margin - 12) / 3;
    SetWindowPos(refresh, nullptr, margin, y, btn_w, 32, SWP_NOZORDER);
    SetWindowPos(browse, nullptr, margin + btn_w + 6, y, btn_w, 32, SWP_NOZORDER);
    SetWindowPos(load, nullptr, margin + 2 * (btn_w + 6), y, btn_w, 32, SWP_NOZORDER);
    y += 38;
    SetWindowPos(unload, nullptr, margin, y, sidebar - 2 * margin, 32, SWP_NOZORDER);
    y += 38;

    HWND model_path = GetDlgItem(hwnd, kCtlModelPath);
    SetWindowPos(model_path, nullptr, margin, y, sidebar - 2 * margin, 28, SWP_NOZORDER);
    y += 34;

    HWND settings = GetDlgItem(hwnd, kCtlSettings);
    SetWindowPos(settings, nullptr, margin, y, sidebar - 2 * margin, 24, SWP_NOZORDER);
    y += 28;

    const int slider_ids[] = {kCtlTemp, kCtlTopP, kCtlTokens, kCtlContext, kCtlThreads};
    const int value_ids[] = {kCtlTempVal, kCtlTopPVal, kCtlTokensVal, kCtlContextVal,
                             kCtlThreadsVal};
    for (int i = 0; i < 5; ++i) {
        HWND val = GetDlgItem(hwnd, value_ids[i]);
        SetWindowPos(val, nullptr, margin, y, 72, 24, SWP_NOZORDER);
        HWND slider = GetDlgItem(hwnd, slider_ids[i]);
        SetWindowPos(slider, nullptr, margin + 78, y, sidebar - 2 * margin - 84, 24, SWP_NOZORDER);
        y += 30;
    }

    const int right = sidebar + 12;
    const int right_w = width - right - margin;
    const int chat_h = std::max(220, height - 170);
    HWND chat = GetDlgItem(hwnd, kCtlChat);
    SetWindowPos(chat, nullptr, right, margin, right_w, chat_h, SWP_NOZORDER);

    const int input_y = margin + chat_h + 8;
    HWND input = GetDlgItem(hwnd, kCtlInput);
    SetWindowPos(input, nullptr, right, input_y, right_w - 420, 36, SWP_NOZORDER);
    const int action_y = input_y;
    const int len = std::min(80, (right_w - 420 - 12) / 5);
    HWND send = GetDlgItem(hwnd, kCtlSend);
    HWND stop = GetDlgItem(hwnd, kCtlStop);
    HWND clear = GetDlgItem(hwnd, kCtlClear);
    HWND help = GetDlgItem(hwnd, kCtlHelp);
    HWND exit = GetDlgItem(hwnd, kCtlExit);
    int x = right + (right_w - 420) + 12;
    SetWindowPos(send, nullptr, x, action_y, len, 36, SWP_NOZORDER);
    x += len + 6;
    SetWindowPos(stop, nullptr, x, action_y, len, 36, SWP_NOZORDER);
    x += len + 6;
    SetWindowPos(clear, nullptr, x, action_y, len, 36, SWP_NOZORDER);
    x += len + 6;
    SetWindowPos(help, nullptr, x, action_y, len, 36, SWP_NOZORDER);
    x += len + 6;
    SetWindowPos(exit, nullptr, x, action_y, len, 36, SWP_NOZORDER);

    HWND status = GetDlgItem(hwnd, kCtlStatus);
    SetWindowPos(status, nullptr, right, input_y + 44, right_w, 28, SWP_NOZORDER);

    set_text(dir, utf8_to_wide(state_cached_model_dir));
}

void update_slider_labels() {
    HWND temp = GetDlgItem(g_window, kCtlTemp);
    HWND topp = GetDlgItem(g_window, kCtlTopP);
    HWND tokens = GetDlgItem(g_window, kCtlTokens);
    HWND context = GetDlgItem(g_window, kCtlContext);
    HWND threads = GetDlgItem(g_window, kCtlThreads);
    set_text(GetDlgItem(g_window, kCtlTempVal),
             format_slider_value("Temperature",
                                 static_cast<int>(SendMessageW(temp, TBM_GETPOS, 0, 0)), 100));
    set_text(GetDlgItem(g_window, kCtlTopPVal),
             format_slider_value("Top P", static_cast<int>(SendMessageW(topp, TBM_GETPOS, 0, 0)),
                                 100));
    set_text(GetDlgItem(g_window, kCtlTokensVal),
             format_value("Tokens", static_cast<int>(SendMessageW(tokens, TBM_GETPOS, 0, 0))));
    set_text(GetDlgItem(g_window, kCtlContextVal),
             format_value("Context", static_cast<int>(SendMessageW(context, TBM_GETPOS, 0, 0))));
    set_text(GetDlgItem(g_window, kCtlThreadsVal),
             format_value("Threads", static_cast<int>(SendMessageW(threads, TBM_GETPOS, 0, 0))));
}

void browse_for_model() {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_window;
    ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        set_text(GetDlgItem(g_window, kCtlModelPath), path);
    }
}

void show_help() {
    std::string text;
    text += "Welcome to SkiffLLM.\r\n\r\n";
    text += "1. Pick a GGUF model or type its path.\r\n";
    text += "2. Click Load Model and wait for Ready.\r\n";
    text += "3. Type a message and press Send.\r\n";
    text += "Use Stop to interrupt generation, Clear to reset the chat.\r\n";
    text += "Click Exit or the X button to close.\r\n";
    append_chat(text);
}

void apply_font_to_children(HWND hwnd) {
    HWND child = nullptr;
    while ((child = FindWindowExW(hwnd, child, nullptr, nullptr)) != nullptr) {
        const int id = GetDlgCtrlID(child);
        const HFONT font = id == kCtlTitle ? g_title_font : g_font;
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiStatePtr state = g_state;
    if (msg == WM_CREATE) {
        g_window = hwnd;
        create_controls(hwnd);
        apply_font_to_children(hwnd);
        layout_controls(hwnd);
        update_slider_labels();
        SetTimer(hwnd, kRefreshTimer, 40, nullptr);
        return 0;
    }
    if (state) {
        switch (msg) {
            case WM_SIZE:
                layout_controls(hwnd);
                return 0;
            case WM_TIMER:
                if (wParam == kRefreshTimer) {
                    update_slider_labels();
                    if (state->dirty.exchange(false)) {
                        update_status_from_state(state);
                    }
                    update_generation_view(state);
                }
                return 0;
            case WM_COMMAND: {
                const int id = LOWORD(wParam);
                const int code = HIWORD(wParam);
                if (id == kCtlRefresh) {
                    refresh_models(state);
                    return 0;
                }
                if (id == kCtlLoad) {
                    start_load(state);
                    return 0;
                }
                if (id == kCtlUnload) {
                    unload_model(state);
                    return 0;
                }
                if (id == kCtlBrowse) {
                    browse_for_model();
                    return 0;
                }
                if (id == kCtlSend) {
                    start_generate(state);
                    return 0;
                }
                if (id == kCtlStop) {
                    stop_generation(state);
                    return 0;
                }
                if (id == kCtlClear) {
                    clear_chat(state);
                    return 0;
                }
                if (id == kCtlHelp) {
                    show_help();
                    return 0;
                }
                if (id == kCtlExit) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (id == kCtlModels && code == LBN_SELCHANGE) {
                    const int index = static_cast<int>(
                        SendMessageW(GetDlgItem(hwnd, kCtlModels), LB_GETCURSEL, 0, 0));
                    {
                        std::lock_guard<std::mutex> guard(state->mutex);
                        state->selected_model = index;
                    }
                    return 0;
                }
                if (id == kCtlModels && code == LBN_DBLCLK) {
                    start_load(state);
                    return 0;
                }
                break;
            }
            case WM_CTLCOLORSTATIC: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, g_background_color);
                SetTextColor(dc, g_text_color);
                const HWND control = reinterpret_cast<HWND>(lParam);
                const int id = GetDlgCtrlID(control);
                if (id == kCtlTitle) {
                    SetTextColor(dc, g_accent_color);
                }
                return reinterpret_cast<LRESULT>(g_background);
            }
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, g_edit_color);
                SetTextColor(dc, g_text_color);
                return reinterpret_cast<LRESULT>(g_edit_background);
            }
            case WM_CTLCOLORBTN: {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetBkColor(dc, g_panel_color);
                return reinterpret_cast<LRESULT>(g_panel);
            }
            case kModelMessage:
                update_status_from_state(state);
                if (wParam) {
                    append_chat_line("Info", "Model loaded.");
                } else {
                    append_chat_line("Error", state->load_error.empty() ? "Model load failed."
                                                                        : state->load_error);
                }
                return 0;
            case kGenerationDone: {
                std::string error;
                {
                    std::lock_guard<std::mutex> guard(state->mutex);
                    error = state->worker_error;
                    state->worker_error.clear();
                    state->stream.clear();
                }
                if (!error.empty()) {
                    append_chat_line("Error", error);
                }
                update_status_from_state(state);
                update_generation_view(state);
            }
                return 0;
            case WM_CLOSE:
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd, kRefreshTimer);
                if (state) {
                    state->stop_requested.store(true);
                    state->engine.reset();
                    state->engine_cfg.reset();
                }
                PostQuitMessage(0);
                return 0;
            default:
                break;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR cmd_line, int) {
    const std::string command_line = cmd_line ? cmd_line : "";
    const bool want_version = command_line.find("--version") != std::string::npos;
    if (want_version) {
        std::puts("SkiffLLM " SKIFFLLM_VERSION);
        return 0;
    }

    INITCOMMONCONTROLSEX init = {};
    init.dwSize = sizeof(init);
    init.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&init);

    g_font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_title_font = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_background = CreateSolidBrush(g_background_color);
    g_panel = CreateSolidBrush(g_panel_color);
    g_edit_background = CreateSolidBrush(g_edit_color);

    GuiStatePtr state = std::make_shared<GuiState>();
    state->cfg = skiffllm::default_config();
    skiffllm::apply_environment(state->cfg);
    {
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (!appdata || !*appdata) {
            const char* userprofile = std::getenv("USERPROFILE");
            appdata = userprofile ? userprofile : "";
        }
        if (appdata && *appdata) {
            state->cfg.model_dir = std::filesystem::path(appdata) / "SkiffLLM" / "models";
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(state->cfg.model_dir, ec);
    state_cached_model_dir = state->cfg.model_dir.string();
    g_state = state;
    llama_backend_init();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SkiffLLMWindow";
    RegisterClassExW(&wc);

    HWND hwnd =
        CreateWindowExW(0, wc.lpszClassName, L"SkiffLLM", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                        CW_USEDEFAULT, 1280, 800, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        return 1;
    }
    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);
    refresh_models(state);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }
        Sleep(10);
    }

    state->stop_requested.store(true);
    state->engine.reset();
    state->engine_cfg.reset();
    DeleteObject(g_font);
    DeleteObject(g_title_font);
    DeleteObject(g_background);
    DeleteObject(g_panel);
    DeleteObject(g_edit_background);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
