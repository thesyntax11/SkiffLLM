#pragma once

#include <string>

namespace skiffllm {

bool bearer_token_matches(const std::string& authorization, const std::string& api_key);

}
