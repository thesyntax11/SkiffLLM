# Changelog

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
