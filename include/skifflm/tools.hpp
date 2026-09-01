#pragma once

#include "skifflm/config.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace skifflm {

struct CatalogModel {
    std::string id;
    std::string name;
    std::string description;
    std::string repo;
    std::string file;
    std::string ram_note;
    uint64_t bytes = 0;
    bool recommended = false;
};

// A small, fully local model catalog. It drives `model list` and `model info`,
// and is also used by model_fetch.py and the Android app. Keep these three
// catalogs in sync when adding a model.
const std::vector<CatalogModel>& model_catalog();
const CatalogModel* find_catalog_model(const std::string& id);

// Build a bounded "project context" block for `--project <dir>`.
// The block is intentionally capped so it fits into a small local context.
std::string build_project_block(const std::filesystem::path& root, std::string& error);

// Run a shell command and capture its stdout. Returns false on failure.
bool run_command(const std::string& command, std::string& output, std::string& error);

// Subcommand entry points. A negative return means "not handled here".
// A non-negative return means the process should exit with that code.
// For "model" this fully handles list/info/install/remove.
// For "git" this either fully handles a command or builds a prompt into
// `cfg.one_shot` and returns -1 so the normal generation path runs.
int handle_model_command(Config& cfg,
                         const std::vector<std::string>& args,
                         std::string& error);
int handle_git_command(Config& cfg,
                       const std::vector<std::string>& args,
                       std::string& error);
int handle_session_command(Config& cfg,
                           const std::vector<std::string>& args,
                           std::string& error);

std::filesystem::path session_file_for(const Config& cfg,
                                       const std::string& name);
std::vector<std::filesystem::path> list_session_files(const Config& cfg,
                                                      std::string& error);
bool write_config_file(const std::filesystem::path& path,
                       const Config& cfg,
                       std::string& error);
int handle_config_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error);
int handle_server_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error);
int handle_chat_template_command(Config& cfg,
                                 const std::vector<std::string>& args,
                                 std::string& error);
int handle_openai_command(Config& cfg,
                          const std::vector<std::string>& args,
                          std::string& error);
std::string load_memories(const Config& cfg);
bool append_memory(const Config& cfg,
                   const std::string& text,
                   std::string& error);
std::filesystem::path metrics_path_for(const Config& cfg);
void record_generation(const Config& cfg,
                       int prompt_tokens,
                       int generated_tokens,
                       double prompt_ms,
                       double generation_ms,
                       double tokens_per_second);
struct UsageStats {
    int sessions = 0;
    uint64_t messages = 0;
    uint64_t prompt_tokens = 0;
    uint64_t generated_tokens = 0;
    double total_prompt_ms = 0.0;
    double total_generation_ms = 0.0;
};
bool load_usage_stats(const Config& cfg, UsageStats& stats, std::string& error);
void print_usage_stats(const Config& cfg, bool as_json);
bool remove_memory(const Config& cfg,
                   const std::string& needle,
                   size_t& removed,
                   std::string& error);
bool clear_memories(const Config& cfg, std::string& error);

bool is_stdin_tty();
std::string read_stdin_all();

}  // namespace skifflm
