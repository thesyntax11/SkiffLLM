#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace skiffllm {

struct Config {
    std::filesystem::path model_path;
    std::filesystem::path model_dir;
    std::filesystem::path config_path;
    std::filesystem::path history_path;
    std::filesystem::path output_path;
    std::filesystem::path export_path;
    std::filesystem::path prompt_file;
    std::filesystem::path project_path;
    std::string system_prompt;
    std::string remember_text;
    std::string forget_text;
    std::filesystem::path memory_path;
    std::filesystem::path summarize_path;
    std::string model_name;
    std::string session_name;
    std::string profile_name;
    std::string chat_template;
    std::vector<std::string> stop_sequences;
    std::vector<std::filesystem::path> attach_paths;

    int context_size = 4096;
    int batch_size = 512;
    int n_ubatch = 0;
    int n_threads = 0;
    int n_gpu_layers = 0;
    int reserve_ctx = 0;
    float temperature = 0.7f;
    float top_p = 0.95f;
    float min_p = 0.0f;
    float typical_p = 0.0f;
    int top_k = 40;
    float repeat_penalty = 1.1f;
    int repeat_last_n = 64;
    int n_predict = 512;
    uint32_t seed = 0xFFFFFFFFu;

    bool use_mmap = true;
    bool use_mlock = false;
    bool flash_attn = false;
    bool numa = false;
    bool offload_kqv = true;
    int n_keep = 0;

    bool color = true;
    bool log_llama = false;
    bool save_history = true;
    bool interactive = true;
    bool show_info = true;
    bool auto_trim = true;
    bool debug = false;
    bool reset_history = false;
    bool show_help = false;
    bool show_version = false;
    bool show_config = false;
    bool list_models = false;
    bool model_info = false;
    bool doctor = false;
    bool doctor_network = false;
    bool smoke = false;
    bool code_mode = false;
    bool warmup = false;
    bool json_output = false;
    bool read_stdin = false;
    bool serve = false;
    std::string server_host = "127.0.0.1";
    int server_port = 8080;
    std::string api_key;
    int benchmark_runs = 0;
    std::string one_shot;
    std::string tokenize_text;

    // UI / diagnostics
    bool context_bar = true;
    bool backend_info = false;
};

Config default_config();
std::filesystem::path expand_path(const std::string& value);
std::string trim(const std::string& value);
std::string lower(std::string value);
std::string to_human_bytes(uint64_t bytes);
std::string to_human_count(uint64_t count);
std::vector<std::filesystem::path> discover_models(const Config& cfg, std::string& error);
bool apply_profile(Config& cfg, const std::string& name, std::string& error);
bool parse_config_file(const std::filesystem::path& path, Config& cfg, std::string& error);
bool parse_args(int argc, char** argv, Config& cfg, std::string& error);
void apply_environment(Config& cfg);
std::string usage(const std::string& program);
void print_config(const Config& cfg, bool as_json = false);

}  // namespace skiffllm
