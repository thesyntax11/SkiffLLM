#include "skiffllm/session.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>

namespace skiffllm {
namespace {

constexpr uint32_t kMagic = 0x534B4946u;
constexpr uint32_t kVersion = 1u;

// Hard caps so a corrupt or malicious history file cannot force an unbounded
// allocation. A real conversation is far below these limits; they only guard
// against files whose length/count fields are bogus.
constexpr uint32_t kMaxStringBytes = 64u * 1024u * 1024u;  // 64 MiB per field
constexpr uint32_t kMaxMessages = 200000u;

template <typename T>
void write_value(std::ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool read_value(std::istream& in, T& value) {
    return static_cast<bool>(in.read(reinterpret_cast<char*>(&value), sizeof(T)));
}

void write_string(std::ostream& out, const std::string& value) {
    const uint32_t size = static_cast<uint32_t>(value.size());
    write_value(out, size);
    out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool read_string(std::istream& in, std::string& value) {
    uint32_t size = 0;
    if (!read_value(in, size)) {
        return false;
    }
    if (size > kMaxStringBytes) {
        return false;
    }
    value.resize(size);
    if (size > 0) {
        in.read(value.data(), static_cast<std::streamsize>(size));
    }
    return static_cast<bool>(in);
}

}  // namespace

Session::Session(const Config& config) : config_(config) {}

bool Session::load(std::string& error) {
    if (config_.reset_history || config_.history_path.empty()) {
        return true;
    }

    std::ifstream input(config_.history_path, std::ios::binary);
    if (!input.is_open()) {
        if (std::filesystem::exists(config_.history_path)) {
            error = "cannot open history file: " + config_.history_path.string();
            return false;
        }
        return true;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    if (!read_value(input, magic) || !read_value(input, version)) {
        error = "invalid history file";
        return false;
    }
    if (magic != kMagic) {
        error = "unrecognized history file format";
        return false;
    }
    if (version != kVersion) {
        error = "unsupported history version: " + std::to_string(version);
        return false;
    }

    if (!read_string(input, system_prompt_)) {
        error = "invalid history file";
        return false;
    }

    uint32_t count = 0;
    if (!read_value(input, count)) {
        error = "invalid history file";
        return false;
    }
    if (count > kMaxMessages) {
        error = "history file declares too many messages";
        return false;
    }

    messages_.clear();
    messages_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        ChatMessage message;
        if (!read_string(input, message.role) || !read_string(input, message.content)) {
            error = "corrupt history file";
            return false;
        }
        messages_.push_back(std::move(message));
    }

    return true;
}

bool Session::save(std::string& error) const {
    if (config_.history_path.empty()) {
        return true;
    }

    std::error_code ec;
    const auto parent = config_.history_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create history directory: " + parent.string();
            return false;
        }
    }

    const std::filesystem::path temporary_path = config_.history_path.string() + ".tmp";
    std::ofstream output(temporary_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot open history file for writing: " + config_.history_path.string();
        return false;
    }

    write_value(output, kMagic);
    write_value(output, kVersion);
    write_string(output, system_prompt_);

    const uint32_t count = static_cast<uint32_t>(messages_.size());
    write_value(output, count);
    for (const auto& message : messages_) {
        write_string(output, message.role);
        write_string(output, message.content);
    }

    if (!output) {
        error = "failed to write history file";
        std::filesystem::remove(temporary_path, ec);
        return false;
    }
    output.close();

    std::filesystem::rename(temporary_path, config_.history_path, ec);
    if (ec) {
        std::error_code remove_error;
        std::filesystem::remove(config_.history_path, remove_error);
        std::filesystem::rename(temporary_path, config_.history_path, ec);
    }
    if (ec) {
        error = "cannot replace history file: " + config_.history_path.string();
        std::filesystem::remove(temporary_path, ec);
        return false;
    }
    return true;
}

void Session::set_system_prompt(const std::string& prompt) {
    system_prompt_ = prompt;
}

const std::string& Session::system_prompt() const {
    return system_prompt_;
}

std::vector<ChatMessage>& Session::messages() {
    return messages_;
}

const std::vector<ChatMessage>& Session::messages() const {
    return messages_;
}

std::vector<ChatMessage> Session::conversation() const {
    std::vector<ChatMessage> result;
    result.reserve(messages_.size() + 1);
    if (!system_prompt_.empty()) {
        result.push_back({"system", system_prompt_});
    }
    result.insert(result.end(), messages_.begin(), messages_.end());
    return result;
}

bool Session::empty() const {
    return messages_.empty();
}

size_t Session::message_count() const {
    return messages_.size();
}

const Config& Session::config() const {
    return config_;
}

}  // namespace skiffllm
