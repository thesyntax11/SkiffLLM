#pragma once

#include <functional>

#include "llm/config.hpp"
#include "llm/engine.hpp"
#include "llm/terminal.hpp"

namespace llm {

int run_server(Config& config, LlmEngine& engine, Terminal& terminal,
               const GenerationOptions& options, const std::function<bool()>& interrupted);

}
