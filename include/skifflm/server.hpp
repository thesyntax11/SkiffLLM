#pragma once

#include "skifflm/config.hpp"
#include "skifflm/engine.hpp"
#include "skifflm/terminal.hpp"

#include <functional>

namespace skifflm {

int run_server(Config& config,
               LlmEngine& engine,
               Terminal& terminal,
               const GenerationOptions& options,
               const std::function<bool()>& interrupted);

}
