#pragma once

#include <string>

namespace skiffllm {

// Returns true when `authorization` carries a valid
// `Authorization: Bearer <api_key>` value for the configured server key.
// An empty `api_key` bypasses protection so unauthenticated local use keeps
// working, matching the `--serve` default.
bool bearer_token_matches(const std::string& authorization, const std::string& api_key);

}  // namespace skiffllm
