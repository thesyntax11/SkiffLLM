# Changelog

## Unreleased

### Desktop

- Added `scripts/model_fetch.py` to fetch a recommended GGUF model once over
  HTTPS. The runtime itself still never downloads or registers models.
- Moved model warmup into the engine so the desktop `--warmup` path and the
  Android app share one optimized warm-up pass.
- Added `/warmup` to re-run the warm-up pass in the interactive shell.
- Added CORS preflight (`OPTIONS`) and CORS headers to the local server.
- Upgraded `scripts/api_client.py` with `--stream`, `--temperature`,
  `--max-tokens`, and a clean CLI.
- Added `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`, and an
  `.editorconfig`.
- Added `docs/FAQ.md`.
- Added CMake presets, a convenience `Makefile`, and `scripts/check-format.sh`.
- Bumped the desktop version to 1.6.0 and added `GET /version` plus model
  details in `GET /health`.

### Android 1.6.0

- Added a native Android app under `android/` built with Kotlin, Jetpack
  Compose, and a llama.cpp JNI bridge.
- Runs supported GGUF models on device.
- Downloads recommended GGUF models from Hugging Face over HTTPS with progress
  and cancel; inference, chat, and export remain offline.
- Streams tokens into a dark chat UI with stop, clear, export, and settings.
- Warms the model after loading to reduce first-answer latency.
- Remembers the last loaded model and reloads it on the next launch.
- Persists conversations locally and restores them on the next launch.
- Adds a model catalog with measured quantities and real generation
  measurements (tokens, time, tokens per second).
- Added an Android model guide with quantified recommendations for phones.

## 1.5.0

- Added `docs/SETUP.md`, `docs/ARCHITECTURE.md`, `docs/RELEASING.md`, and
  `docs/LIMITATIONS.md`.
- Added `scripts/ci-local.sh` for full local CI without GitHub.
- Added `scripts/release.sh` for local release archives.
- Added `scripts/api_client.py` as a dependency-free server client.
- Extended `scripts/setup.sh` with install and source-dir options.
- Made the README badges and status honest about local CI and limitations.
- Fixed the context budget so `--reserve-ctx` actually reserves generation
  space and the model never overruns the window mid-generation.
- Made interactive streaming fall back to plain output when a line would wrap,
  so live counters no longer corrupt narrow terminals or long paragraphs.
- Stopped echoing entire attached files in one-shot mode; the terminal now
  shows a concise prompt preview.
- Made the local server read headers in bulk instead of one byte at a time.
- Linked the Windows socket library explicitly for the local server.

## 1.4.0

- Added a local OpenAI-compatible HTTP server with `--serve`, `/health`,
  `/v1/models`, and `/v1/chat/completions` including streaming responses.
- Added `--host` and `--port` server binding controls.
- Added `--benchmark <runs>` with real measured prompt and generation timing.
- Generation now reserves the exact context budget and stops before the
  context window fills instead of failing mid-generation.
- Session history writes are now atomic using a temporary file and rename.
- Added CLI tests for server and benchmark flags.

## 1.3.0

- Added file context through `--attach`, `/file`, and `@file` expansion.
- Added Markdown conversation export through `--export` and `/export`.
- Added `--chat-template` model prompt format override.
- Added `--warmup` to reduce first-answer latency.
- Added live token counters in interactive streaming output.
- Added optional GNU Readline editing and history, with getline fallback.
- Expanded command-line and config-file tests.

## 1.2.0

- Added flash attention, NUMA, and KV offload controls.
- Added `--n-keep` to control trimming behavior.
- Added `--doctor` system diagnostics.
- Added `--model-info` metadata inspection.
- Added `--tokenize` and `/tokenize`.
- Added `--smoke` end-to-end generation test.
- Added `/settings` interactive command.
- Filled in JSON output for error paths.
- Expanded tests and CI entry points.

## 1.1.0

- Added named sessions with `--session`.
- Added sampling profiles: balanced, fast, creative, code, precise.
- Added min-p and typical-p sampling controls.
- Added configurable ubatch size.
- Added reserved context space for generation.
- Added automatic history trimming when the context is full.
- Added stop sequences.
- Added JSON output mode.
- Added answer output files.
- Added `--list-models`.
- Added `--no-banner`.
- Added shell completions for bash, zsh, and fish.
- Added example config file.
- Added CI matrix for Linux GCC, Linux Clang, and macOS.
- Added install/export rules and package config.
- Added more CLI tests and entry-point tests.
- Updated llama.cpp pin.

## 1.0.0

- Initial public release.
- Fully offline local LLM terminal assistant built on llama.cpp.
- Interactive chat with streaming output.
- Single prompt mode and stdin support.
- Automatic GGUF model discovery.
- Persistent session history.
- Config file and environment variable support.
- Slash commands.
- GPU layer offload configuration.
- Sampling parameter controls.
- Unit tests and CMake integration.
