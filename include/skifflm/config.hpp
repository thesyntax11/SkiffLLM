#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace skifflm {

struct Config {
    std::filesystem::path model_path;
    std::filesystem::path model_dir;
    std::filesystem::path config_path;
    std::filesystem::path history_path;
    std::string system_prompt;
    std::string model_name;
    int context_size = 4096;
    int batch_size = 512;
    int n_threads = 0;
    int n_gpu_layers = 0;
    float temperature = 0.7f;
    float top_p = 0.95f;
    int top_k = 40;
    float repeat_penalty = 1.1f;
    int repeat_last_n = 64;
    int n_predict = 512;
    uint32_t seed = 0xFFFFFFFFu;
    bool use_mmap = true;
    bool use_mlock = false;
    bool color = true;
    bool log_llama = false;
    bool save_history = true;
    bool interactive = true;
    bool show_info = true;
    bool debug = false;
    bool reset_history = false;
    bool show_help = false;
    bool show_version = false;
    bool show_config = false;
    bool read_stdin = false;
    std::string one_shot;
    std::filesystem::path prompt_file;
};

Config default_config();
std::filesystem::path expand_path(const std::string& value);
std::string trim(const std::string& value);
std::string lower(std::string value);
std::string to_human_bytes(uint64_t bytes);
std::string to_human_count(uint64_t count);
std::vector<std::filesystem::path> discover_models(const Config& cfg, std::string& error);
bool parse_config_file(const std::filesystem::path& path, Config& cfg, std::string& error);
bool parse_args(int argc, char** argv, Config& cfg, std::string& error);
void apply_environment(Config& cfg);
std::string usage(const std::string& program);
void print_config(const Config& cfg);

}
