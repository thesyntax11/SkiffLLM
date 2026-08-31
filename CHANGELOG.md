# Changelog

## 1.4.0

- Added a local OpenAI-compatible HTTP server with `--serve`, `/health`,
  `/v1/models`, and `/v1/chat/completions` including streaming responses.
- Added `--host` and `--port` server binding controls.
- Added `--benchmark <runs>` with real measured prompt and generation timing.
- Generation now reserves the exact context budget and stops before the
  context window fills instead of failing mid-generation.
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
