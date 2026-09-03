#pragma once

#include <string>
#include <vector>

namespace llm {

struct ChatMessage {
    std::string role;
    std::string content;
};

}  // namespace llm
