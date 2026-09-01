# Architecture

## Components

- `include/skifflm/config.hpp` and `src/config.cpp`
  Parse CLI flags, config files, environment variables, profiles, and model
  discovery.

- `include/skifflm/http_auth.hpp` and `src/http_auth.cpp`
  Small, testable HTTP Bearer-token matcher used to protect the local
  `/v1/*` endpoints when `--api-key` is configured.

- `include/skifflm/engine.hpp` and `src/engine.cpp`
  Own the llama.cpp model and context. Build chat prompts, tokenize, run the
  sampler chain, decode tokens, stop early, and report real timing.

- `include/skifflm/session.hpp` and `src/session.cpp`
  Manage the system prompt and conversation history with atomic local writes.

- `include/skifflm/terminal.hpp` and `src/terminal.cpp`
  Handle color output, streaming counters, readline input, help, and stats.

- `include/skifflm/server.hpp` and `src/server.cpp`
  Local HTTP endpoint with `/health`, `/version`, `/v1/models`, and
  `/v1/chat/completions`, including streamed responses, CORS support, and
  optional Bearer auth on `/v1/*`.

- `src/main.cpp`
  CLI entry point: dispatch diagnostics, export, one-shot, interactive,
  benchmark, and server modes.

- `android/app/src/main/java/com/skifflm/app/`
  Android UI and application logic: Compose chat, conversation persistence,
  model catalog, downloader, and the JNI bridge.

- `android/app/src/main/cpp/jni_bridge.cpp`
  JNI surface for the desktop core's `LlmEngine`, exposed to Kotlin through
  `SkiffNative`.

- `scripts/model_fetch.py` and `scripts/api_client.py`
  Optional user-run helpers for fetching a recommended GGUF and for exercising
  the local API. They are not part of the runtime.

Data flows:

1. Parse configuration into `Config`.
2. Load one GGUF model into `LlmEngine`.
3. Build messages into a chat prompt with a template.
4. Run the sampler and decode token by token.
5. Return text, timing, and token counts to the caller.

## Safety and determinism

- Desktop runtime is local only; core inference never calls out. The explicit
  `openai` subcommand is the single runtime path that transmits a prompt to a
  server you choose; `--serve` only opens a listener.
- Android uses network access only for the explicit model downloader.
- Output paths and model paths are resolved locally.
- History writes use a temporary file and rename.
- Generation budgets account for the exact context window.
- Downloads validate GGUF headers and free storage. Catalog byte sizes are
  advisory: a maintainer can re-upload a revision with a different size, and the
  SHA-256 sidecar is the authoritative integrity check.

## Adding a feature

Keep the public API small. Add behavior in `Config` when it affects parsing or
routing. Keep inference in `LlmEngine`. Keep presentation in `Terminal`. Add a
test in `tests/test_main.cpp` and a CTest entry in `CMakeLists.txt` for any
new entry point.
