#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "skifflm/messages.hpp"

namespace skifflm::cli {

double parse_double(const std::string& text, bool& ok);
int parse_int(const std::string& text, bool& ok);
std::string read_file(const std::filesystem::path& path);
std::string read_stdin();
bool is_space(char ch);
std::string make_attach_block(const std::vector<std::filesystem::path>& paths, std::string& error);
std::string expand_at_paths(const std::string& text, std::string& error);
std::string compact_preview(const std::string& text, std::size_t limit = 200);
std::string export_markdown(const std::vector<ChatMessage>& messages);
std::string json_escape(const std::string& value);
bool write_text_file(const std::filesystem::path& path, const std::string& content,
                     std::string& error);

}  // namespace skifflm::cli
