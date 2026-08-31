# Architecture

## Components

- `include/skifflm/config.hpp` and `src/config.cpp`
  Parse CLI flags, config files, environment variables, profiles, and model
  discovery.

- `include/skifflm/engine.hpp` and `src/engine.cpp`
  Own the llama.cpp model and context. Build chat prompts, tokenize, run the
  sampler chain, decode tokens, stop early, and report real timing.

- `include/skifflm/session.hpp` and `src/session.cpp`
  Manage the system prompt and conversation history with atomic local writes.

- `include/skifflm/terminal.hpp` and `src/terminal.cpp`
  Handle color output, streaming counters, readline input, help, and stats.

- `include/skifflm/server.hpp` and `src/server.cpp`
  Local HTTP endpoint with `/health`, `/v1/models`, and
  `/v1/chat/completions`, including streamed responses.

- `src/main.cpp`
  CLI entry point: dispatch diagnostics, export, one-shot, interactive,
  benchmark, and server modes.

Data flows:

1. Parse configuration into `Config`.
2. Load one GGUF model into `LlmEngine`.
3. Build messages into a chat prompt with a template.
4. Run the sampler and decode token by token.
5. Return text, timing, and token counts to the caller.

## Safety and determinism

- Runtime is local only. There are no outbound API calls.
- Output paths and model paths are resolved locally.
- History writes use a temporary file and rename.
- Generation budgets account for the exact context window.

## Adding a feature

Keep the public API small. Add behavior in `Config` when it affects parsing or
routing. Keep inference in `LlmEngine`. Keep presentation in `Terminal`. Add a
test in `tests/test_main.cpp` and a CTest entry in `CMakeLists.txt` for any
new entry point.
