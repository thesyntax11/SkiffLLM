#include "skiffllm/cli_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "skiffllm/config.hpp"

namespace skiffllm::cli {

double parse_double(const std::string& text, bool& ok) {
    ok = false;
    if (text.empty()) {
        return 0.0;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return 0.0;
    }
    ok = true;
    return value;
}

int parse_int(const std::string& text, bool& ok) {
    ok = false;
    if (text.empty()) {
        return 0;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') {
        return 0;
    }
    ok = true;
    return static_cast<int>(value);
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string read_stdin() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

bool is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

std::string make_attach_block(const std::vector<std::filesystem::path>& paths, std::string& error) {
    std::ostringstream output;
    for (const auto& path : paths) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
            error = "attached file does not exist: " + path.string();
            return {};
        }
        const std::string content = read_file(path);
        if (content.empty()) {
            error = "attached file is empty or unreadable: " + path.string();
            return {};
        }
        output << "<file path=\"" << path.string() << "\">\n" << content << "\n</file>\n\n";
    }
    return output.str();
}

std::string expand_at_paths(const std::string& text, std::string& error) {
    std::ostringstream output;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '@') {
            output << text[i];
            i += 1;
            continue;
        }
        size_t end = i + 1;
        while (end < text.size() && !is_space(text[end])) {
            end += 1;
        }
        const std::string token = text.substr(i, end - i);
        const std::string raw_path = token.substr(1);
        if (!raw_path.empty()) {
            const bool path_like = raw_path.find('.') != std::string::npos ||
                                   raw_path.find('/') != std::string::npos ||
                                   raw_path.find('\\') != std::string::npos;
            std::error_code ec;
            const std::filesystem::path path = skiffllm::expand_path(raw_path);
            if (std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec)) {
                const std::string content = read_file(path);
                if (!content.empty()) {
                    output << "<file path=\"" << path.string() << "\">\n"
                           << content << "\n</file>\n\n";
                    i = end;
                    continue;
                }
            } else if (path_like) {
                error = "referenced file does not exist in expansion: " + raw_path;
                return {};
            }
        }
        output << text[i];
        i += 1;
    }
    return output.str();
}

std::string compact_preview(const std::string& text, std::size_t limit) {
    std::string result;
    result.reserve(std::min(text.size(), limit + 1));
    for (const char ch : text) {
        if (ch == '\n' || ch == '\r' || ch == '\t') {
            result.push_back(' ');
        } else {
            result.push_back(ch);
        }
        if (result.size() >= limit) {
            result += "...";
            break;
        }
    }
    return result;
}

std::string export_markdown(const std::vector<ChatMessage>& messages) {
    std::ostringstream output;
    output << "# SkiffLLM Conversation\n\n";
    for (const auto& message : messages) {
        const std::string role = message.role == "system"      ? "System"
                                 : message.role == "user"      ? "User"
                                 : message.role == "assistant" ? "Assistant"
                                                               : message.role;
        output << "## " << role << "\n\n" << message.content << "\n\n";
    }
    return output.str();
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    char buffer[8] = {0};
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                    out << buffer;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    out << '"';
    return out.str();
}

bool write_text_file(const std::filesystem::path& path, const std::string& content,
                     std::string& error) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = "cannot create output directory: " + parent.string();
            return false;
        }
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error = "cannot open output file for writing: " + path.string();
        return false;
    }
    output << content;
    if (!output) {
        error = "failed to write output file: " + path.string();
        return false;
    }
    return true;
}

}
