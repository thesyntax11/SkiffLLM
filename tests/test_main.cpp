#include "skifflm/config.hpp"
#include "skifflm/session.hpp"
#include "skifflm/tools.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

bool approx(float left, float right) {
    const float diff = left - right;
    return diff > -0.0001f && diff < 0.0001f;
}

std::vector<char*> args_from(std::vector<std::string>& storage) {
    std::vector<char*> result;
    for (auto& value : storage) {
        result.push_back(value.data());
    }
    return result;
}

void test_config_file() {
    const auto dir = std::filesystem::temp_directory_path() / "skifflm-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / "config.txt";

    {
        std::ofstream out(path);
        out << "ctx=2048\n";
        out << "temp=0.42\n";
        out << "top-p=0.9\n";
        out << "min-p=0.05\n";
        out << "no-color\n";
        out << "system=Be concise.\n";
        out << "stop=END\n";
        out << "reserve-ctx=128\n";
        out << "export=out.md\n";
        out << "attach=notes.txt\n";
        out << "chat-template=chatml\n";
        out << "warmup\n";
    }

    skifflm::Config cfg = skifflm::default_config();
    std::string error;
    const bool ok = skifflm::parse_config_file(path, cfg, error);
    check(ok, "config file should parse");
    check(cfg.context_size == 2048, "ctx should be 2048");
    check(approx(cfg.temperature, 0.42f), "temperature should be 0.42");
    check(approx(cfg.top_p, 0.9f), "top_p should be 0.9");
    check(approx(cfg.min_p, 0.05f), "min_p should be 0.05");
    check(!cfg.color, "color should be disabled");
    check(cfg.system_prompt == "Be concise.", "system prompt should be set");
    check(cfg.stop_sequences.size() == 1, "stop sequence should be stored");
    check(cfg.stop_sequences[0] == "END", "stop sequence should match");
    check(cfg.reserve_ctx == 128, "reserve_ctx should be 128");
    check(cfg.export_path == "out.md", "export path should be stored");
    check(cfg.attach_paths.size() == 1, "attach path should be stored");
    check(cfg.attach_paths[0] == "notes.txt", "attach path should match");
    check(cfg.chat_template == "chatml", "chat template should be stored");
    check(cfg.warmup, "warmup should be enabled");

    std::filesystem::remove_all(dir);
}

void test_parse_args() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {
        "skifflm",
        "--model=/tmp/model.gguf",
        "--ctx",
        "1024",
        "--temp=0.31",
        "--repeat-penalty",
        "1.05",
        "--no-color",
        "--seed",
        "1234",
    };
    auto args = args_from(storage);

    std::string error;
    const bool ok = skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error);
    check(ok, "arguments should parse");
    check(cfg.model_path == "/tmp/model.gguf", "model path should be set");
    check(cfg.context_size == 1024, "ctx should be 1024");
    check(approx(cfg.temperature, 0.31f), "temperature should be 0.31");
    check(approx(cfg.repeat_penalty, 1.05f), "repeat penalty should be 1.05");
    check(!cfg.color, "color should be disabled");
    check(cfg.seed == 1234u, "seed should be 1234");
}

void test_profiles() {
    skifflm::Config cfg = skifflm::default_config();
    std::string error;

    check(skifflm::apply_profile(cfg, "fast", error), "fast profile should apply");
    check(approx(cfg.temperature, 0.60f), "fast temperature");
    check(approx(cfg.top_p, 0.90f), "fast top_p");
    check(cfg.top_k == 30, "fast top_k");
    check(cfg.n_predict == 256, "fast n_predict");

    check(skifflm::apply_profile(cfg, "code", error), "code profile should apply");
    check(approx(cfg.temperature, 0.20f), "code temperature");
    check(cfg.n_predict == 1024, "code n_predict");

    check(!skifflm::apply_profile(cfg, "missing", error), "unknown profile should fail");
    check(!error.empty(), "unknown profile should set an error");
}

void test_parse_args_new_features() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {
        "skifflm",
        "--session",
        "writing",
        "--profile=creative",
        "--stop",
        "END",
        "--stop=OK",
        "--min-p",
        "0.08",
        "--typical",
        "0.5",
        "--ubatch",
        "256",
        "--reserve-ctx",
        "96",
        "--json",
        "--no-banner",
    };
    auto args = args_from(storage);

    std::string error;
    const bool ok = skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error);
    check(ok, "new feature arguments should parse");
    check(cfg.session_name == "writing", "session name should be set");
    check(cfg.json_output, "json output should be enabled");
    check(!cfg.interactive, "json mode should disable interactive mode");
    check(!cfg.show_info, "no-banner should disable info");
    check(cfg.stop_sequences.size() == 2, "two stop sequences should be stored");
    check(cfg.stop_sequences[0] == "END", "first stop sequence");
    check(cfg.stop_sequences[1] == "OK", "second stop sequence");
    check(approx(cfg.min_p, 0.08f), "min-p should be set");
    check(approx(cfg.typical_p, 0.5f), "typical should be set");
    check(cfg.n_ubatch == 256, "ubatch should be set");
    check(cfg.reserve_ctx == 96, "reserve_ctx should be set");
    check(cfg.profile_name == "creative", "profile name should be set");
    check(approx(cfg.temperature, 1.0f), "creative temperature should apply");
}

void test_session_roundtrip() {
    const auto dir = std::filesystem::temp_directory_path() / "skifflm-session";
    std::filesystem::create_directories(dir);
    const auto path = dir / "history.skif";

    skifflm::Config cfg = skifflm::default_config();
    cfg.history_path = path;
    cfg.reset_history = true;

    {
        skifflm::Session session(cfg);
        session.set_system_prompt("You are a helper.");
        auto& messages = session.messages();
        messages.push_back({"user", "Hello"});
        messages.push_back({"assistant", "Hi there"});

        std::string error;
        check(session.save(error), "session should save");
        check(error.empty(), "session save error should be empty");
    }

    skifflm::Config cfg2 = skifflm::default_config();
    cfg2.history_path = path;

    skifflm::Session session(cfg2);
    std::string error;
    check(session.load(error), "session should load");
    check(error.empty(), "session load error should be empty");
    check(session.system_prompt() == "You are a helper.", "system prompt should roundtrip");
    check(session.message_count() == 2, "message count should be 2");
    check(session.messages()[0].role == "user", "first message role should be user");
    check(session.messages()[0].content == "Hello", "first message content should roundtrip");
    check(session.messages()[1].role == "assistant", "second message role should be assistant");
    check(session.messages()[1].content == "Hi there", "second message content should roundtrip");

    const auto conversation = session.conversation();
    check(conversation.size() == 3, "conversation should include system message");
    check(conversation[0].role == "system", "conversation should start with system role");
    check(conversation[0].content == "You are a helper.", "system prompt should be first");

    std::filesystem::remove_all(dir);
}

void test_formatting() {
    check(skifflm::trim("  hello \t\n") == "hello", "trim should remove whitespace");
    check(skifflm::lower("AbC") == "abc", "lower should lowercase");
    check(skifflm::to_human_bytes(1024u) == "1.00 KiB", "bytes should format");
    check(skifflm::to_human_count(1500u) == "1.50 K", "count should format");
}

void test_advanced_flags() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {
        "skifflm",
        "--flash-attn",
        "--numa",
        "--no-kv-offload",
        "--n-keep",
        "3",
        "--doctor",
        "--model-info",
        "--smoke",
        "--tokenize",
        "hello world",
    };
    auto args = args_from(storage);
    std::string error;
    bool ok = skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error);
    check(ok, "advanced flags should parse");
    check(cfg.flash_attn, "flash_attn should be enabled");
    check(cfg.numa, "numa should be enabled");
    check(!cfg.offload_kqv, "kv offload should be disabled");
    check(cfg.n_keep == 3, "n_keep should be 3");
    check(cfg.doctor, "doctor should be enabled");
    check(cfg.model_info, "model_info should be enabled");
    check(cfg.smoke, "smoke should be enabled");
    check(cfg.tokenize_text == "hello world", "tokenize text should be stored");
    check(!cfg.interactive, "model_info should disable interactive mode");
}

void test_popularity_flags() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {
        "skifflm",
        "--attach",
        "notes.txt",
        "--file=guide.md",
        "--export",
        "chat.md",
        "--chat-template",
        "chatml",
        "--warmup",
    };
    auto args = args_from(storage);
    std::string error;
    const bool ok = skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error);
    check(ok, "popularity flags should parse");
    check(cfg.attach_paths.size() == 2, "attach paths should be stored");
    check(cfg.attach_paths[0] == "notes.txt", "first attach path");
    check(cfg.attach_paths[1] == "guide.md", "second attach path");
    check(cfg.export_path == "chat.md", "export path should be stored");
    check(cfg.chat_template == "chatml", "chat template should be stored");
    check(cfg.warmup, "warmup should be enabled");
}

void test_tools_features() {
    skifflm::Config cfg = skifflm::default_config();

    // Unix-style positional prompt handling.
    {
        std::vector<std::string> storage = {"skifflm", "explain this code"};
        auto args = args_from(storage);
        std::string error;
        check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
              "positional prompt should parse");
        check(cfg.one_shot == "explain this code", "positional word becomes the prompt");
        check(cfg.model_path.empty(), "a non-path word must not become the model");
        check(!cfg.interactive, "a positional prompt should disable interactive mode");
    }

    // Memory and convenience shorthand flags.
    {
        skifflm::Config memory_cfg = skifflm::default_config();
        std::vector<std::string> storage = {"skifflm", "--remember", "be concise"};
        auto args = args_from(storage);
        std::string parse_error;
        check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), memory_cfg, parse_error),
              "remember flag should parse");
        check(memory_cfg.remember_text == "be concise", "remember text should be stored");

        memory_cfg = skifflm::default_config();
        storage = {"skifflm", "--forget", "concise"};
        args = args_from(storage);
        check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), memory_cfg, parse_error),
              "forget flag should parse");
        check(memory_cfg.forget_text == "concise", "forget text should be stored");

        memory_cfg = skifflm::default_config();
        storage = {"skifflm", "--summarize", "README.md"};
        args = args_from(storage);
        check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), memory_cfg, parse_error),
              "summarize flag should parse");
        check(memory_cfg.summarize_path == "README.md", "summarize path should be stored");
        check(!memory_cfg.interactive, "summarize should disable interactive mode");
    }

    // Session file resolution keeps session names tidy.
    {
        skifflm::Config session_cfg = skifflm::default_config();
        skifflm::Config custom = session_cfg;
        custom.history_path = std::filesystem::temp_directory_path() / "skifflm-sessions" / "default.skif";

        const auto path = skifflm::session_file_for(custom, "writing");
        check(path.filename() == "writing.skif", "session file uses the sanitized name");
        check(path.parent_path() == custom.history_path.parent_path(),
              "session file lives beside history");
    }

    // Safe code mode is a flag, not a file-mutating feature.
    {
        skifflm::Config code_cfg = skifflm::default_config();
        std::vector<std::string> storage = {"skifflm", "--code", "--project", ".", "fix it"};
        auto args = args_from(storage);
        std::string parse_error;
        check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), code_cfg, parse_error),
              "code flag should parse");
        check(code_cfg.code_mode, "code mode should be enabled");
        check(code_cfg.project_path == ".", "project path should be stored");
        check(code_cfg.one_shot == "fix it", "positional prompt should be retained");
    }

    // Project context is bounded but includes the real map and source slice.
    const auto dir = std::filesystem::temp_directory_path() / "skifflm-project-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "src");
    std::filesystem::create_directories(dir / ".git");
    {
        std::ofstream(dir / "src" / "main.cpp") << "int main() { return 0; }\n";
        std::ofstream(dir / "src" / "test_main.cpp") << "void run() {}\n";
        std::ofstream(dir / "config.yaml") << "name: test\n";
        std::ofstream(dir / ".git" / "head") << "fake\n";
    }
    std::string error;
    const std::string block = skifflm::build_project_block(dir, error);
    check(!error.empty() || !block.empty(), "project block should build");
    check(block.find("source files:") != std::string::npos, "project block reports source count");
    check(block.find("main.cpp") != std::string::npos, "project block includes the file index");
    check(block.find("<file path=\"src/main.cpp\">") != std::string::npos,
          "project block includes a source slice");
    check(block.find(".git") == std::string::npos, "project block skips vendor/hidden dirs");
    std::filesystem::remove_all(dir);

    check(skifflm::model_catalog().size() >= 5, "model catalog should be non-empty");
    check(skifflm::find_catalog_model("qwen2.5-0.5b") != nullptr,
          "catalog should find a known id");
}

void test_config_and_stats_features() {
    const auto dir = std::filesystem::temp_directory_path() / "skifflm-config-stats-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    skifflm::Config cfg = skifflm::default_config();
    cfg.config_path = dir / "config";
    cfg.history_path = dir / "history.skif";
    cfg.model_path = "qwen2.5-0.5b.gguf";
    cfg.stop_sequences = {"END", "STOP"};
    std::string error;

    check(skifflm::write_config_file(cfg.config_path, cfg, error), "config should be written");
    check(std::filesystem::exists(cfg.config_path), "config path should exist");
    int keys = 0;
    {
        std::ifstream input(cfg.config_path);
        std::string line;
        while (std::getline(input, line)) {
            if (line.find('=') != std::string::npos) {
                ++keys;
            }
        }
    }
    check(keys >= 20, "written config should include the main options");
    check(skifflm::metrics_path_for(cfg) == (dir / "metrics.txt"),
          "metrics file should live next to history");

    skifflm::record_generation(cfg, 10, 50, 100.0, 500.0, 100.0);
    skifflm::record_generation(cfg, 5, 20, 80.0, 320.0, 62.5);

    skifflm::UsageStats stats;
    check(skifflm::load_usage_stats(cfg, stats, error), "usage stats should load");
    check(stats.sessions == 2, "usage stats should count two generations");
    check(stats.messages == 85, "usage stats should count prompt+generated tokens");
    check(stats.prompt_tokens == 15, "usage stats should total prompt tokens");
    check(stats.generated_tokens == 70, "usage stats should total generated tokens");
    check(approx(static_cast<float>(stats.total_prompt_ms), 180.0f),
          "usage stats should total prompt milliseconds");
    check(approx(static_cast<float>(stats.total_generation_ms), 820.0f),
          "usage stats should total generation milliseconds");

    std::filesystem::remove_all(dir);
}

void test_server_and_benchmark_flags() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {
        "skifflm",
        "--serve",
        "--host",
        "0.0.0.0",
        "--port",
        "9090",
        "--benchmark",
        "3",
    };
    auto args = args_from(storage);
    std::string error;
    const bool ok = skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error);
    check(ok, "server and benchmark flags should parse");
    check(cfg.serve, "serve should be enabled");
    check(cfg.server_host == "0.0.0.0", "server host should be stored");
    check(cfg.server_port == 9090, "server port should be stored");
    check(cfg.benchmark_runs == 3, "benchmark runs should be stored");
    check(!cfg.interactive, "serve should disable interactive mode");
}

void test_session_resolution() {
    skifflm::Config cfg = skifflm::default_config();
    std::vector<std::string> storage = {"skifflm", "--session", "writing"};
    auto args = args_from(storage);
    std::string error;
    check(skifflm::parse_args(static_cast<int>(args.size()), args.data(), cfg, error),
          "session argument should parse");
    check(cfg.session_name == "writing", "session name should be retained");
    check(!cfg.history_path.empty(), "session should resolve a history path");
    check(cfg.history_path.filename() == "writing.skif", "session history should use the name");
}

}

int main() {
    test_config_file();
    test_parse_args();
    test_profiles();
    test_parse_args_new_features();
    test_session_roundtrip();
    test_formatting();
    test_session_resolution();
    test_advanced_flags();
    test_popularity_flags();
    test_server_and_benchmark_flags();
    test_tools_features();
    test_config_and_stats_features();
    return 0;
}
