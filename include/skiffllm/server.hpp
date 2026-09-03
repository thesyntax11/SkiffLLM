#pragma once

#include <functional>

#include "skiffllm/config.hpp"
#include "skiffllm/engine.hpp"
#include "skiffllm/terminal.hpp"

namespace skiffllm {

int run_server(Config& config, SkiffEngine& engine, Terminal& terminal,
               const GenerationOptions& options, const std::function<bool()>& interrupted);

}
