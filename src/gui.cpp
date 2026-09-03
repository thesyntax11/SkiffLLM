#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0;
static UINT g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

namespace {

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
    std::string load_error;
    std::vector<ChatMessage> messages;
    std::string draft;
    std::string stream;
    std::string worker_error;
    std::atomic<bool> generating{false};
    std::atomic<bool> stop_requested{false};
    std::string status;
};

using GuiStatePtr = std::shared_ptr<GuiState>;

std::string trim(std::string value) {
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

void refresh_models(GuiStatePtr state) {
    std::string error;
    std::vector<std::filesystem::path> models = skiffllm::discover_models(state->cfg, error);
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->models = std::move(models);
        state->selected_model = 0;
        if (!error.empty()) {
            state->status = error;
        } else if (state->models.empty()) {
            state->status = "No models found";
        } else {
            state->status = "Select a model";
        }
    }
}

void stop_generation(GuiStatePtr state) {
    state->stop_requested.store(true);
    std::lock_guard<std::mutex> guard(state->mutex);
    state->status = "Stopping generation...";
}

void start_load(GuiStatePtr state) {
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (state->loading.load() || state->loaded.load()) {
            return;
        }
        std::string model_path;
        if (state->selected_model >= 0 &&
            state->selected_model < static_cast<int>(state->models.size())) {
            model_path = state->models[static_cast<size_t>(state->selected_model)].string();
        }
        const std::string custom = trim(state->custom_model);
        if (!custom.empty()) {
            model_path = skiffllm::expand_path(custom).string();
        }
        if (model_path.empty()) {
            state->load_error = "Choose a model first.";
            state->status = "Model load failed";
            return;
        }
        state->cfg.model_path = model_path;
        state->loading.store(true);
        state->loaded.store(false);
        state->load_error.clear();
        state->status = "Loading model...";
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
        }
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
}

void start_generate(GuiStatePtr state) {
    std::vector<ChatMessage> ask;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        if (!state->loaded.load() || state->generating.load() || state->loading.load()) {
            return;
        }
        const std::string input = trim(state->draft);
        if (input.empty()) {
            return;
        }
        state->messages.push_back({"user", input});
        ask = state->messages;
        state->draft.clear();
        state->stream.clear();
        state->worker_error.clear();
        state->generating.store(true);
        state->stop_requested.store(false);
        state->status = "Generating...";
    }
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
            std::lock_guard<std::mutex> guard(shared->mutex);
            shared->stream += part;
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
        }
    }).detach();
}

void clear_chat(GuiStatePtr state) {
    std::lock_guard<std::mutex> guard(state->mutex);
    state->messages.clear();
    state->stream.clear();
    state->worker_error.clear();
    state->status = "Conversation cleared";
}

void CreateRenderTarget();
void CleanupRenderTarget();

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT create_device_flags = 0;
    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_level_array[2] = {D3D_FEATURE_LEVEL_11_0,
                                                      D3D_FEATURE_LEVEL_10_0};
    HRESULT res =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                      create_device_flags, feature_level_array, 2,
                                      D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
                                      &feature_level, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                            create_device_flags, feature_level_array, 2,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
                                            &feature_level, &g_pd3dDeviceContext);
    }
    if (res != S_OK) {
        return false;
    }
    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D* back_buffer = nullptr;
    if (g_pSwapChain) {
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    }
    if (back_buffer) {
        g_pd3dDevice->CreateRenderTargetView(back_buffer, nullptr, &g_mainRenderTargetView);
        back_buffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    wchar_t win_dir[MAX_PATH] = {};
    const UINT len = GetWindowsDirectoryW(win_dir, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        const std::filesystem::path fonts = std::filesystem::path(win_dir) / L"Fonts";
        const std::filesystem::path segoe = fonts / L"segoeui.ttf";
        if (std::filesystem::exists(segoe)) {
            io.FontDefault = io.Fonts->AddFontFromFileTTF(segoe.string().c_str(), 16.0f);
        }
    }
    if (!io.FontDefault) {
        io.FontDefault = io.Fonts->AddFontDefault();
    }
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 7.0f;
    style.ChildRounding = 10.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(18.0f, 18.0f);
    style.FramePadding = ImVec2(14.0f, 8.0f);
    style.ItemSpacing = ImVec2(12.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 14.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.13f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.11f, 0.17f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.12f, 0.18f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.18f, 0.22f, 0.32f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.14f, 0.21f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.20f, 0.30f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.25f, 0.36f, 1.0f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.10f, 0.16f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.16f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.10f, 0.16f, 0.8f);
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.18f, 0.28f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.26f, 0.40f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.24f, 0.31f, 0.46f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.15f, 0.22f, 0.42f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.32f, 0.58f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.40f, 0.68f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(0.42f, 0.72f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.60f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.48f, 0.72f, 1.0f, 1.0f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.93f, 0.98f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.57f, 0.66f, 1.0f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.09f, 0.13f, 0.9f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.30f, 0.42f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.40f, 0.55f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.50f, 0.68f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.24f, 0.30f, 0.42f, 1.0f);
}

void DrawModelPanel(GuiStatePtr state) {
    std::shared_ptr<SkiffEngine> engine;
    std::string load_error;
    std::string status;
    char custom_input[1024] = {};
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        engine = state->engine;
        load_error = state->load_error;
        status = state->status;
        std::snprintf(custom_input, sizeof(custom_input), "%s", state->custom_model.c_str());
    }
    ImGui::TextColored(ImVec4(0.42f, 0.72f, 1.0f, 1.0f), "SkiffLLM");
    ImGui::TextDisabled("Offline LLM Desktop");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Models");
    ImGui::TextDisabled("%s", state->cfg.model_dir.string().c_str());
    if (!state->models.empty()) {
        const int selected = state->selected_model;
        std::string preview = selected >= 0 && selected < static_cast<int>(state->models.size())
                                  ? state->models[static_cast<size_t>(selected)].filename().string()
                                  : "No model";
        ImGui::PushItemWidth(-1.0f);
        if (ImGui::BeginCombo("##model", preview.c_str())) {
            for (int i = 0; i < static_cast<int>(state->models.size()); ++i) {
                const bool picked = i == state->selected_model;
                const std::string label = state->models[static_cast<size_t>(i)].filename().string();
                if (ImGui::Selectable(label.c_str(), picked)) {
                    state->selected_model = i;
                }
                if (picked) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
    } else {
        ImGui::TextWrapped("No GGUF models found in this directory.");
    }

    ImGui::Spacing();
    ImGui::Text("Custom path");
    ImGui::PushItemWidth(-1.0f);
    ImGui::InputText("##custommodel", custom_input, IM_ARRAYSIZE(custom_input));
    ImGui::PopItemWidth();
    state->custom_model = custom_input;

    if (state->loading.load()) {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.30f, 1.0f), "Loading model...");
    } else if (state->loaded.load() && engine) {
        const ModelInfo& info = engine->info();
        ImGui::TextColored(ImVec4(0.42f, 0.86f, 0.55f, 1.0f), "Ready");
        ImGui::TextDisabled("Context %u", engine->context_capacity());
        if (!info.description.empty()) {
            ImGui::TextDisabled("%s", info.description.c_str());
        }
        if (info.n_params != 0) {
            ImGui::TextDisabled("Params %llu", info.n_params);
        }
    } else {
        ImGui::TextColored(ImVec4(0.95f, 0.52f, 0.52f, 1.0f), "Not loaded");
    }
    if (!load_error.empty()) {
        ImGui::TextWrapped("%s", load_error.c_str());
    }

    if (state->loaded.load()) {
        if (ImGui::Button("Unload Model", ImVec2(-1.0f, 0.0f))) {
            unload_model(state);
        }
    } else {
        ImGui::BeginDisabled(state->loading.load());
        if (ImGui::Button("Load Model", ImVec2(-1.0f, 0.0f))) {
            start_load(state);
        }
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Refresh Models", ImVec2(-1.0f, 0.0f))) {
        refresh_models(state);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("Generation Settings");
    ImGui::PushItemWidth(-1.0f);
    ImGui::SliderFloat("Temperature", &state->cfg.temperature, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Top P", &state->cfg.top_p, 0.0f, 1.0f, "%.2f");
    ImGui::SliderInt("Max Tokens", &state->cfg.n_predict, 16, 4096);
    ImGui::SliderInt("Context", &state->cfg.context_size, 512, 32768);
    ImGui::SliderInt("GPU Layers", &state->cfg.n_gpu_layers, 0, 99);
    ImGui::SliderInt("Threads", &state->cfg.n_threads, 1, 64);
    ImGui::PopItemWidth();
    ImGui::TextDisabled("Settings apply when a model is loaded.");
    ImGui::TextDisabled("%s", status.c_str());
}

void DrawChatPanel(GuiStatePtr state) {
    std::vector<ChatMessage> messages;
    std::string stream;
    std::string worker_error;
    std::string status;
    char draft_input[8192] = {};
    const bool loaded = state->loaded.load();
    const bool generating = state->generating.load();
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        messages = state->messages;
        stream = state->stream;
        worker_error = state->worker_error;
        status = state->status;
        std::snprintf(draft_input, sizeof(draft_input), "%s", state->draft.c_str());
    }

    ImGui::BeginChild("chat_history", ImVec2(0.0f, -96.0f), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 8.0f);
    if (messages.empty() && !generating) {
        ImGui::TextColored(ImVec4(0.62f, 0.68f, 0.78f, 1.0f),
                           "Start a conversation. Select and load a model on the left.");
    }
    for (const ChatMessage& message : messages) {
        ImGui::Spacing();
        if (message.role == "user") {
            ImGui::TextColored(ImVec4(0.42f, 0.72f, 1.0f, 1.0f), "You");
            ImGui::TextWrapped("%s", message.content.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.42f, 0.86f, 0.55f, 1.0f), "SkiffLLM");
            ImGui::TextWrapped("%s", message.content.c_str());
        }
    }
    if (generating) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.42f, 0.72f, 1.0f, 1.0f), "SkiffLLM");
        ImGui::TextWrapped("%s", stream.c_str());
    }
    if (!worker_error.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.52f, 0.52f, 1.0f), "%s", worker_error.c_str());
    }
    ImGui::PopTextWrapPos();
    if (generating) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.09f, 0.11f, 0.17f, 1.0f));
    ImGui::BeginChild("input_area", ImVec2(0.0f, 0.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextColored(ImVec4(0.62f, 0.68f, 0.78f, 1.0f), "Message");
    ImGui::PushItemWidth(-100.0f);
    const bool submit =
        ImGui::InputText("##message", draft_input, sizeof(draft_input),
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopItemWidth();
    state->draft = draft_input;
    ImGui::SameLine();
    ImGui::BeginDisabled(!loaded || generating);
    if (ImGui::Button("Send", ImVec2(0.0f, 0.0f)) || submit) {
        start_generate(state);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (generating) {
        if (ImGui::Button("Stop")) {
            stop_generation(state);
        }
    } else {
        ImGui::BeginDisabled(state->draft.empty());
        if (ImGui::Button("Clear")) {
            clear_chat(state);
        }
        ImGui::EndDisabled();
    }
    ImGui::PopStyleColor();
    ImGui::EndChild();

    ImGui::TextDisabled("%s", status.c_str());
}

void RenderApp(GuiStatePtr state) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::Begin("SkiffLLM", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar(3);

    ImGui::BeginChild("sidebar", ImVec2(320.0f, 0.0f), true);
    DrawModelPanel(state);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("main", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollWithMouse);
    DrawChatPanel(state);
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) {
                return 0;
            }
            g_ResizeWidth = LOWORD(lParam);
            g_ResizeHeight = HIWORD(lParam);
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR cmd_line, int) {
    const std::string command_line = cmd_line ? cmd_line : "";
    const bool want_version = command_line.find("--version") != std::string::npos;
    if (want_version) {
        std::puts("SkiffLLM " SKIFFLLM_VERSION);
        return 0;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
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

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ApplyStyle();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    LoadFonts();

    GuiStatePtr state = std::make_shared<GuiState>();
    state->cfg = skiffllm::default_config();
    skiffllm::apply_environment(state->cfg);
    {
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (appdata && *appdata) {
            state->cfg.model_dir = std::filesystem::path(appdata) / "SkiffLLM" / "models";
        }
    }
    refresh_models(state);
    llama_backend_init();

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        if (g_SwapChainOccluded &&
            g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = 0;
            g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderApp(state);
        ImGui::Render();

        const float clear_color[4] = {0.06f, 0.08f, 0.13f, 1.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
