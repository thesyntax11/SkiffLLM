#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "skiffllm/config.hpp"

namespace skiffllm {

struct SkillRequest {
    std::string name;
    std::map<std::string, std::string> args;
};

bool parse_skill_requests(const std::string& text, std::vector<SkillRequest>& requests,
                          std::string& error);
std::string strip_skill_markers(const std::string& text);
std::string skill_instructions(const std::vector<std::string>& enabled, const Config& cfg);
bool skill_available(const std::string& name, const std::vector<std::string>& enabled);
bool path_in_scope(const Config& cfg, const std::filesystem::path& path, bool require_target,
                   std::string& error);
std::vector<std::string> skill_catalog();
std::string skill_description(const std::string& name);
std::string skill_example(const std::string& name);
std::string execute_skill(const Config& cfg, const SkillRequest& request, std::string& error);

}
