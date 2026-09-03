#pragma once

#include <string>
#include <vector>

namespace skiffllm {

struct ChatMessage {
    std::string role;
    std::string content;
};

}
