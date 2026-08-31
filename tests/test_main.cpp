#include "skifflm/config.hpp"
#include "skifflm/session.hpp"

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

void test_config_file() {
    const auto dir = std::filesystem::temp_directory_path() / "skifflm-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / "config.txt";

    {
        std::ofstream out(path);
        out << "ctx=2048\n";
        out << "temp=0.42\n";
        out << "top-p=0.9\n";
        out << "no-color\n";
        out << "system=Be concise.\n";
    }

    skifflm::Config cfg = skifflm::default_config();
    std::string error;
    const bool ok = skifflm::parse_config_file(path, cfg, error);
    check(ok, "config file should parse");
    check(cfg.context_size == 2048, "ctx should be 2048");
    check(approx(cfg.temperature, 0.42f), "temperature should be 0.42");
    check(approx(cfg.top_p, 0.9f), "top_p should be 0.9");
    check(!cfg.color, "color should be disabled");
    check(cfg.system_prompt == "Be concise.", "system prompt should be set");

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
    std::vector<char*> args;
    for (auto& value : storage) {
        args.push_back(value.data());
    }

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

}

int main() {
    test_config_file();
    test_parse_args();
    test_session_roundtrip();
    test_formatting();
    return 0;
}
