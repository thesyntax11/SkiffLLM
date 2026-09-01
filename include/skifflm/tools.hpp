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

bool is_stdin_tty();
std::string read_stdin_all();

}  // namespace skifflm
