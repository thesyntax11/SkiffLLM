#include "skifflm/tools.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#include <unistd.h>
#define POPEN popen
#define PCLOSE pclose
#endif

namespace skifflm {
namespace {

bool is_hidden_or_build_dir(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    if (name.empty()) {
        return false;
    }
    if (name == ".git" || name == ".hg" || name == ".svn" || name == "build" ||
        name == "build2" || name == "node_modules" || name == "vendor" ||
        name == "_deps" || name == ".gradle" || name == "dist" || name == "out" ||
        name == "target" || name == ".venv" || name == "venv" ||
        name == "__pycache__" || name == ".next" || name == ".cache" ||
        name == ".pytest_cache" || name == ".idea" || name == ".vscode") {
        return true;
    }
    return name.size() > 1 && name[0] == '.';
}

bool is_source(const std::string& ext) {
    static const std::vector<std::string> exts = {
        "cpp", "cc", "cxx", "c", "hpp", "hxx", "hh", "h", "kt", "java",
        "py", "rs", "go", "js", "ts", "tsx", "sh", "cmake"};
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool is_config(const std::string& ext) {
    static const std::vector<std::string> exts = {
        "json", "yaml", "yml", "toml", "ini", "conf", "cfg", "xml",
        "properties", "md", "txt"};
    return std::find(exts.begin(), exts.end(), ext) != exts.end();
}

bool looks_like_test(const std::filesystem::path& relative) {
    const std::string lower = [&]() {
        std::string value = relative.string();
        std::transform(value.begin(), value.end(), value.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }();
    return lower.find("test") != std::string::npos ||
           lower.find("spec") != std::string::npos ||
           lower.find("_test") != std::string::npos ||
           lower.find("/tests/") != std::string::npos ||
           lower.find("/test_") != std::string::npos;
}

std::string extension_of(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return ext;
}

struct ProjectFile {
    std::filesystem::path relative;
    uint64_t bytes = 0;
};

bool run_git(const std::string& arguments, std::string& output, std::string& error) {
    return run_command("git " + arguments, output, error);
}

std::string instruction_for_git(const std::string& sub) {
    if (sub == "review") {
        return "You are a senior code reviewer. Review the diff below for bugs, "
               "security issues, error handling problems, and style regressions. "
               "Answer with a short prioritized list: HIGH, MEDIUM or LOW per finding, "
               "include file and line when available, and finish with one line of overall "
               "verdict. Be concrete; do not invent findings.";
    }
    if (sub == "explain") {
        return "Explain the diff below in plain language. Summarize what changed, "
               "why it matters, and call out anything risky. Keep it concise.";
    }
    if (sub == "commit") {
        return "Write a conventional commit message below the diff. The diff is "
               "below. Reply with only the commit subject and body in Git "
               "format, no extra commentary.";
    }
    return "Answer the user's request using the git input below as context.";
}

}  // namespace

const std::vector<CatalogModel>& model_catalog() {
    static const std::vector<CatalogModel> catalog = {
        {"qwen2.5-0.5b", "Qwen2.5 0.5B Instruct",
         "Best first model on older machines; fast and small.",
         "Qwen/Qwen2.5-0.5B-Instruct-GGUF", "qwen2.5-0.5b-instruct-q4_k_m.gguf",
         "~1 GB working set", 491400032, true},
        {"qwen3-0.6b", "Qwen3 0.6B Instruct",
         "Small Qwen3 with optional thinking mode; good quality per byte.",
         "bartowski/Qwen_Qwen3-0.6B-GGUF", "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
         "~1 GB working set", 484220320, true},
        {"llama3.2-1b", "Llama 3.2 1B Instruct",
         "Balanced quality and speed for typical CPUs.",
         "bartowski/Llama-3.2-1B-Instruct-GGUF", "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
         "~1.5 GB working set", 807694464, true},
        {"smollm2-1.7b", "SmolLM2 1.7B Instruct",
         "Mid-size option with a good quality-to-speed ratio.",
         "bartowski/SmolLM2-1.7B-Instruct-GGUF", "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
         "~2 GB working set", 1055609824, false},
        {"phi3.5-mini", "Phi-3.5 Mini Instruct",
         "Better reasoning; needs roughly 4 GB working set.",
         "bartowski/Phi-3.5-mini-instruct-GGUF", "Phi-3.5-mini-instruct-Q4_K_M.gguf",
         "~4 GB working set", 2393232672, false},
    };
    return catalog;
}

const CatalogModel* find_catalog_model(const std::string& id) {
    for (const auto& model : model_catalog()) {
        if (model.id == id) {
            return &model;
        }
    }
    return nullptr;
}

std::string build_project_block(const std::filesystem::path& root, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        error = "project directory does not exist: " + root.string();
        return {};
    }

    std::vector<ProjectFile> files;
    uint64_t total_source = 0;
    uint64_t total_tests = 0;
    uint64_t total_config = 0;

    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) {
            break;
        }
        const auto path = it->path();
        const auto relative = std::filesystem::relative(path, root, ec);
        if (ec) {
            continue;
        }
        if (relative.empty()) {
            continue;
        }
        for (const auto& part : relative) {
            if (is_hidden_or_build_dir(part)) {
                it.disable_recursion_pending();
                break;
            }
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        const std::string ext = extension_of(path);
        const uint64_t size = static_cast<uint64_t>(it->file_size(ec));
        if (ec) {
            continue;
        }
        files.push_back({relative, size});
        if (is_source(ext)) {
            total_source += 1;
        }
        if (is_config(ext)) {
            total_config += 1;
        }
        if (looks_like_test(relative)) {
            total_tests += 1;
        }
    }

    std::sort(files.begin(), files.end(), [](const ProjectFile& a, const ProjectFile& b) {
        return a.relative < b.relative;
    });

    std::ostringstream out;
    out << "<project root=\"" << root.string() << "\">\n";
    out << "<summary>\n";
    out << "files: " << files.size() << "\n";
    out << "source files: " << total_source << "\n";
    out << "test files: " << total_tests << "\n";
    out << "config files: " << total_config << "\n";
    out << "</summary>\n";
    out << "<file-index>\n";

    const size_t max_index = 600;
    for (size_t i = 0; i < files.size() && i < max_index; ++i) {
        out << files[i].relative.string() << " (" << files[i].bytes << " bytes)\n";
    }
    if (files.size() > max_index) {
        out << "... " << (files.size() - max_index) << " more files\n";
    }
    out << "</file-index>\n";

    // Include a bounded slice of the most useful source and config files.
    const size_t max_included = 20;
    const size_t per_file_limit = 6000;
    const size_t total_limit = 60000;
    size_t included = 0;
    size_t total = 0;
    for (const auto& file : files) {
        if (included >= max_included || total >= total_limit) {
            break;
        }
        const std::string ext = extension_of(file.relative);
        if (!is_source(ext) && !is_config(ext)) {
            continue;
        }
        std::ifstream input(root / file.relative, std::ios::binary);
        if (!input.is_open()) {
            continue;
        }
        std::string content;
        content.reserve(std::min<uint64_t>(file.bytes, per_file_limit));
        char buffer[4096];
        while (content.size() < per_file_limit && input.read(buffer, sizeof(buffer))) {
            content.append(buffer, static_cast<size_t>(input.gcount()));
        }
        input.close();
        out << "<file path=\"" << file.relative.string() << "\">\n";
        out << content.substr(0, per_file_limit);
        out << "\n</file>\n";
        included += 1;
        total += content.size();
    }
    out << "</project>\n";
    return out.str();
}

bool run_command(const std::string& command, std::string& output, std::string& error) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        error = "cannot start command: " + command;
        return false;
    }
    char buffer[4096];
    output.clear();
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int status = PCLOSE(pipe);
    if (status != 0) {
        error = "command exited with status " + std::to_string(status) + ": " + command;
        return false;
    }
    return true;
}

int handle_model_command(Config& cfg,
                         const std::vector<std::string>& args,
                         std::string& error) {
    const std::string action = args.empty() ? "list" : args[0];
    std::error_code ec;
    std::filesystem::create_directories(cfg.model_dir, ec);

    auto installed = [&](const CatalogModel& model) -> bool {
        if (cfg.model_dir.empty()) {
            return false;
        }
        return std::filesystem::exists(cfg.model_dir / model.file, ec);
    };

    if (action == "list" || action == "ls") {
        std::cout << std::left;
        std::cout << std::setw(16) << "ID"
                  << std::setw(24) << "NAME"
                  << std::setw(12) << "SIZE"
                  << std::setw(24) << "RAM"
                  << "INSTALLED\n";
        std::cout << std::string(96, '-') << "\n";
        for (const auto& model : model_catalog()) {
            std::cout << std::setw(16) << model.id
                      << std::setw(24) << model.name.substr(0, 23)
                      << std::setw(12) << to_human_bytes(model.bytes)
                      << std::setw(24) << model.ram_note
                      << (installed(model) ? "yes" : "no") << "\n";
        }
        std::cout << "\nInstall with: skifflm model install <id>\n";
        return 0;
    }

    if (action == "info" || action == "show") {
        if (args.size() < 2) {
            error = "usage: skifflm model info <id>";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        std::cout << "ID:          " << model->id << "\n";
        std::cout << "Name:        " << model->name << "\n";
        std::cout << "Description: " << model->description << "\n";
        std::cout << "Repo:       " << model->repo << "\n";
        std::cout << "File:       " << model->file << "\n";
        std::cout << "Size:       " << to_human_bytes(model->bytes) << "\n";
        std::cout << "RAM:        " << model->ram_note << "\n";
        std::cout << "Installed:  " << (installed(*model) ? "yes" : "no") << "\n";
        return 0;
    }

    if (action == "install" || action == "get") {
        if (args.size() < 2) {
            error = "usage: skifflm model install <id>";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        if (installed(*model)) {
            std::cout << model->file << " is already installed at "
                      << (cfg.model_dir / model->file).string() << "\n";
            return 0;
        }

        // Find the fetch helper next to the installed binary or in the source
        // checkout.
        const std::filesystem::path project_root =
            std::filesystem::current_path();
        std::vector<std::filesystem::path> candidates = {
            project_root / "scripts" / "model_fetch.py",
        };
        bool found = false;
        std::filesystem::path helper;
        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate, ec) &&
                std::filesystem::is_regular_file(candidate, ec)) {
                helper = candidate;
                found = true;
                break;
            }
        }
        if (!found) {
            error =
                "model_fetch.py not found. Install a model with:\n"
                "  python3 scripts/model_fetch.py --model " + model->id +
                " --output-dir " + cfg.model_dir.string();
            return 1;
        }

        const std::string command =
            "python3 \"" + helper.string() + "\" --model " + model->id +
            " --output-dir \"" + cfg.model_dir.string() + "\"";
        const int status = std::system(command.c_str());
        if (status != 0) {
            error = "model download failed for " + model->id;
            return 1;
        }
        std::cout << model->file << " installed in " << cfg.model_dir.string() << "\n";
        return 0;
    }

    if (action == "remove" || action == "rm") {
        if (args.size() < 2) {
            error = "usage: skifflm model remove <id> [--force]";
            return 2;
        }
        const CatalogModel* model = find_catalog_model(args[1]);
        if (model == nullptr) {
            error = "unknown model id: " + args[1] + " (use `skifflm model list`)";
            return 2;
        }
        const std::filesystem::path target = cfg.model_dir / model->file;
        if (!std::filesystem::exists(target, ec)) {
            std::cout << model->file << " is not installed.\n";
            return 0;
        }
        const bool force = std::find(args.begin(), args.end(), "--force") != args.end();
        if (!force) {
            std::cout << target.string() << "\n"
                      << "Re-run with --force to delete this file.\n";
            return 0;
        }
        if (!std::filesystem::remove(target, ec) || ec) {
            error = "could not remove " + target.string() + ": " + ec.message();
            return 1;
        }
        std::cout << "Removed " << model->file << "\n";
        return 0;
    }

    error = "unknown model action: " + action +
            " (use list, info, install or remove)";
    return 2;
}

int handle_git_command(Config& cfg,
                       const std::vector<std::string>& args,
                       std::string& error) {
    const std::string action = args.empty() ? "diff" : args[0];
    bool use_cached = false;
    std::string git_args;

    if (action == "diff" || action == "review" || action == "explain" ||
        action == "commit") {
        use_cached = std::find(args.begin(), args.end(), "--cached") != args.end();
        git_args = use_cached ? "diff --cached" : "diff";
        if (action == "commit" && !use_cached) {
            std::cout << "No --cached diff provided. Use `skifflm git commit --cached` "
                      << "after staging changes.\n";
            return 1;
        }
        std::string diff;
        if (!run_git(git_args, diff, error)) {
            return 1;
        }
        if (diff.empty()) {
            std::cout << "No diff to analyze.\n";
            return 0;
        }
        std::string instruction = instruction_for_git(action);
        cfg.one_shot = instruction + "\n\n<git input>\n" + diff + "\n</git input>\n";
        cfg.interactive = false;
        return -1;  // continue to generation
    }

    if (action == "log") {
        std::string log;
        if (!run_git("log -n 40 --oneline", log, error)) {
            return 1;
        }
        cfg.one_shot =
            "Summarize this git history into a short release-note style outline.\n\n"
            "<git log>\n" + log + "\n</git log>\n";
        cfg.interactive = false;
        return -1;
    }

    if (action == "status") {
        std::string status;
        if (!run_git("status --short", status, error)) {
            return 1;
        }
        std::cout << status;
        return 0;
    }

    error = "unknown git action: " + action +
            " (use diff, review, explain, commit, log or status)";
    return 2;
}

bool is_stdin_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

std::string read_stdin_all() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

}  // namespace skifflm
