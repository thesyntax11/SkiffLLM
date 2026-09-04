#include <cstdlib>
#ifdef _WIN32
#include <stdlib.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "skiffllm/cli_utils.hpp"
#include "skiffllm/config.hpp"
#include "skiffllm/http_auth.hpp"
#include "skiffllm/skills.hpp"
#include "skiffllm/tools.hpp"

namespace {

void extra_check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

bool extra_approx(float left, float right) {
    const float diff = left - right;
    return diff > -0.0001f && diff < 0.0001f;
}

std::vector<char*> extra_args_from(std::vector<std::string>& storage) {
    std::vector<char*> result;
    for (auto& value : storage) {
        result.push_back(value.data());
    }
    return result;
}

void set_env_value(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value == nullptr ? "" : value);
#else
    if (value == nullptr) {
        unsetenv(name);
    } else {
        setenv(name, value, 1);
    }
#endif
}

void test_parse_args_errors() {
    skiffllm::Config cfg = skiffllm::default_config();
    std::vector<std::string> storage = {"skiffllm", "--unknown-flag"};
    auto args = extra_args_from(storage);
    std::string error;
    extra_check(!skiffllm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
                "unknown flag should fail");
    extra_check(!error.empty(), "unknown flag should report an error");

    storage = {"skiffllm", "--ctx"};
    args = extra_args_from(storage);
    error.clear();
    extra_check(!skiffllm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
                "missing value should fail");
    extra_check(!error.empty(), "missing value should report an error");

    storage = {"skiffllm", "--json=yes"};
    args = extra_args_from(storage);
    error.clear();
    extra_check(!skiffllm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
                "boolean flag should reject an unexpected value");
}

void test_parse_config_errors() {
    const auto dir = std::filesystem::temp_directory_path() / "skiffllm-extra-config";
    std::filesystem::create_directories(dir);
    const auto path = dir / "bad.txt";
    {
        std::ofstream out(path);
        out << "temp=not-a-number\n";
    }
    skiffllm::Config cfg = skiffllm::default_config();
    std::string error;
    extra_check(!skiffllm::parse_config_file(path, cfg, error), "invalid config value should fail");
    extra_check(!error.empty(), "invalid config value should report an error");
    std::filesystem::remove_all(dir);
}

void test_attach_and_expand() {
    const auto dir = std::filesystem::temp_directory_path() / "skiffllm-extra-attach";
    std::filesystem::create_directories(dir);
    const auto path = dir / "notes.txt";
    {
        std::ofstream out(path);
        out << "memory content\n";
    }

    std::string error;
    const std::string block = skiffllm::cli::make_attach_block({path}, error);
    extra_check(!error.empty() || !block.empty(), "attach block should be available");
    extra_check(block.find("memory content") != std::string::npos,
                "attach block should include file content");

    error.clear();
    const std::string expanded =
        skiffllm::cli::expand_at_paths("@" + path.string() + " rest", error);
    extra_check(!error.empty() || !expanded.empty(), "at-path expansion should succeed");
    extra_check(expanded.find("memory content") != std::string::npos,
                "at-path expansion should include file content");
    extra_check(expanded.find("rest") != std::string::npos,
                "at-path expansion should preserve trailing text");

    error.clear();
    const std::string missing = skiffllm::cli::expand_at_paths("@missing/notes.txt", error);
    extra_check(missing.empty() && !error.empty(), "missing at-file should fail with an error");

    error.clear();
    extra_check(skiffllm::cli::write_text_file(dir / "nested" / "out.txt", "abc", error),
                "write_text_file should create parent directories");
    extra_check(skiffllm::cli::read_file(dir / "nested" / "out.txt") == "abc",
                "write_text_file should roundtrip content");

    std::filesystem::remove_all(dir);
}

void test_json_escape_control() {
    const std::string escaped = skiffllm::cli::json_escape(std::string("a\b\f\n\r\t\x1f") + "\"\\");
    extra_check(escaped.find("\\u001f") != std::string::npos,
                "json_escape should escape control characters");
    extra_check(escaped.find("\\n") != std::string::npos, "json_escape should escape newlines");
    extra_check(escaped.find("\\\"") != std::string::npos, "json_escape should escape quotes");
    extra_check(escaped.find("\\\\") != std::string::npos, "json_escape should escape backslashes");
}

void test_session_sanitization() {
    skiffllm::Config cfg = skiffllm::default_config();
    cfg.history_path =
        std::filesystem::temp_directory_path() / "skiffllm-extra-session" / "default.skif";
    const auto path = skiffllm::session_file_for(cfg, "project notes?");
    extra_check(path.filename() != "project notes?.skif", "session name should be sanitized");
    extra_check(path.extension() == ".skif", "session file should keep the .skif suffix");
}

void test_memory_roundtrip() {
    const auto dir = std::filesystem::temp_directory_path() / "skiffllm-extra-memory";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    skiffllm::Config cfg = skiffllm::default_config();
    cfg.memory_path = dir / "memories.txt";

    std::string error;
    extra_check(skiffllm::append_memory(cfg, "keep first", error), "append memory should succeed");
    extra_check(skiffllm::append_memory(cfg, "keep second", error),
                "append second memory should succeed");
    const std::string memories = skiffllm::load_memories(cfg);
    extra_check(memories.find("keep first") != std::string::npos,
                "memory file should contain the first memory");
    extra_check(memories.find("keep second") != std::string::npos,
                "memory file should contain the second memory");

    size_t removed = 0;
    extra_check(skiffllm::remove_memory(cfg, "first", removed, error),
                "remove memory should succeed");
    extra_check(removed == 1, "remove memory should count one match");
    const std::string after = skiffllm::load_memories(cfg);
    extra_check(after.find("keep first") == std::string::npos,
                "removed memory line should be gone");
    extra_check(after.find("keep second") != std::string::npos,
                "unmatched memory line should remain");

    extra_check(skiffllm::clear_memories(cfg, error), "clear memories should succeed");
    extra_check(skiffllm::load_memories(cfg).empty(), "cleared memory file should be empty");
    std::filesystem::remove_all(dir);
}

void test_usage_stats_empty() {
    const auto dir = std::filesystem::temp_directory_path() / "skiffllm-extra-metrics";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    skiffllm::Config cfg = skiffllm::default_config();
    cfg.history_path = dir / "history.skif";
    std::string error;
    skiffllm::UsageStats stats;
    extra_check(skiffllm::load_usage_stats(cfg, stats, error),
                "missing metrics file should not fail");
    extra_check(stats.sessions == 0, "missing metrics file should report zero sessions");
    extra_check(stats.messages == 0, "missing metrics file should report zero messages");
    std::filesystem::remove_all(dir);
}

void test_project_block_empty() {
    const auto dir = std::filesystem::temp_directory_path() / "skiffllm-extra-project";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::string error;
    const std::string block = skiffllm::build_project_block(dir, error);
    extra_check(!error.empty() || !block.empty(),
                "empty project should either report an error or return a block");
    std::filesystem::remove_all(dir);
}

void test_human_formats() {
    extra_check(skiffllm::to_human_bytes(0u) == "0 B", "zero bytes should format");
    extra_check(skiffllm::to_human_bytes(1024u * 1024u) == "1.00 MiB", "mebibytes should format");
    extra_check(skiffllm::to_human_count(1000000u) == "1.00 M", "millions should format");
    extra_check(skiffllm::trim(" \t\r\n ") == "", "trim should handle only whitespace");
    extra_check(skiffllm::lower("SKIFFLLM") == "skiffllm", "lower should normalize text");
}

void test_default_config_paths() {
    const skiffllm::Config cfg = skiffllm::default_config();
    extra_check(cfg.model_dir.string().find("skiffllm") != std::string::npos,
                "model directory should use the skiffllm name");
    extra_check(cfg.config_path.string().find("skiffllm") != std::string::npos,
                "config path should use the skiffllm name");
    extra_check(cfg.history_path.string().find("skiffllm") != std::string::npos,
                "history path should use the skiffllm name");
    extra_check(extra_approx(cfg.temperature, 0.7f), "default temperature should be 0.7");
    extra_check(cfg.context_size == 4096, "default context should be 4096");
}

void test_catalog_edges() {
    extra_check(skiffllm::find_catalog_model("does-not-exist") == nullptr,
                "unknown catalog id should return null");
    extra_check(skiffllm::model_catalog().size() >= 5, "catalog should expose multiple models");
}

void test_skills_config() {
    const auto catalog = skiffllm::skill_catalog();
    extra_check(!catalog.empty(), "skill catalog should not be empty");
    for (const auto& name : catalog) {
        extra_check(!skiffllm::skill_description(name).empty(),
                    "every skill should have a description");
        extra_check(!skiffllm::skill_example(name).empty(), "every skill should have an example");
    }
    skiffllm::Config cfg = skiffllm::default_config();
    extra_check(!cfg.skills_enabled, "skills should be off by default");
    extra_check(cfg.enabled_skills.empty(), "no skill should be enabled by default");

    std::vector<std::string> storage = {"skiffllm", "--skills", "--enable-skill", catalog.front(),
                                        "--show-config"};
    auto args = extra_args_from(storage);
    std::string error;
    extra_check(skiffllm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
                "skills flags should parse");
    extra_check(cfg.skills_enabled, "--skills should enable skills");
    extra_check(cfg.enabled_skills.size() == 1 && cfg.enabled_skills.front() == catalog.front(),
                "--enable-skill should add the skill");

    const std::filesystem::path config_path =
        std::filesystem::temp_directory_path() / "skiffllm-test-skill.cfg";
    std::error_code ec;
    std::filesystem::remove(config_path, ec);
    extra_check(skiffllm::write_config_file(config_path, cfg, error),
                "skill config should be writable");
    skiffllm::Config loaded = skiffllm::default_config();
    extra_check(skiffllm::parse_config_file(config_path, loaded, error),
                "written skill config should be readable");
    extra_check(loaded.skills_enabled, "written skill toggle should round trip");
    extra_check(loaded.enabled_skills == cfg.enabled_skills,
                "written skill list should round trip");
    std::filesystem::remove(config_path, ec);
}

void test_environment() {
    const char* old_model = std::getenv("SKIFFLLM_MODEL");
    const char* old_api = std::getenv("SKIFFLLM_API_KEY");
    const char* old_server = std::getenv("SKIFFLLM_SERVER_KEY");

    set_env_value("SKIFFLLM_MODEL", "/tmp/skiffllm.gguf");
    set_env_value("SKIFFLLM_API_KEY", "key-from-env");
    set_env_value("SKIFFLLM_SERVER_KEY", nullptr);

    skiffllm::Config cfg = skiffllm::default_config();
    skiffllm::apply_environment(cfg);
    extra_check(cfg.model_path == "/tmp/skiffllm.gguf", "environment should set the model");
    extra_check(cfg.api_key == "key-from-env", "environment should set the api key");

    set_env_value("SKIFFLLM_MODEL", old_model);
    set_env_value("SKIFFLLM_API_KEY", old_api);
    set_env_value("SKIFFLLM_SERVER_KEY", old_server);
}

}

void run_extra_tests() {
    test_parse_args_errors();
    test_parse_config_errors();
    test_attach_and_expand();
    test_json_escape_control();
    test_session_sanitization();
    test_memory_roundtrip();
    test_usage_stats_empty();
    test_project_block_empty();
    test_human_formats();
    test_default_config_paths();
    test_catalog_edges();
    test_skills_config();
    test_environment();
}
