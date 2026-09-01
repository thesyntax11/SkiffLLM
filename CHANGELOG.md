# Changelog

## Unreleased

### Desktop

- Added `skifflm run "<prompt>" [--ctx N --temp T --threads N]` as the primary
  one-shot CLI entry point.
- Added `--context-bar` / `--no-context-bar` and a live context-usage progress
  bar after each interactive generation.
- Added `--backend-info` and `-DSKIFFLLM_LLAMA_BACKEND=cuda|metal|vulkan|opencl|blas`
  so hardware acceleration is explicitly opt-in and observable.
- Added `skifflm chat-template list|detect|info` for automatic template
  discovery and `skifflm model verify <id> [--update]` for GGUF size/header and
  SHA-256 integrity checks.
- `model_fetch.py` now writes a `.gguf.sha256` sidecar and supports
  `--verify` and `--checksum`.
- Random sampling seeds are resolved per generation when `--seed random`.
- Added `release-cuda|vulkan|metal|opencl|blas` CMake presets and backward
  Android `-Pskifflm.backend=` GPU builds.
- Added `skifflm openai "..."` as a zero-dependency OpenAI-compatible client
  with `--stream`, `--no-json`, `--temp`, and `--max-tokens`.
- Android: multi-conversation management (create/open/delete from `Chats`).
- Unified model catalog IDs across the desktop catalog, `model_fetch.py`, and
  the Android catalog.
- Added an iOS SwiftUI client (`ios/`) with a llama.cpp Objective-C++ bridge,
  token streaming, auto-trim, context bar, themes, multi-conversation,
  persistent facts, quick prompts, sampling profiles and stop sequences,
  model warm-up, typical-p/seed sampling, GGUF import from Files, a Share
  Extension for incoming text, Markdown export, and HTTPS model downloads
  with SHA-256 verification. Setup: `scripts/ios-setup.sh`.
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
- Expanded the project layout and offline-guarantee documentation.
- Updated limitations for optional model downloads and Android.
- Added Unix-pipeline ergonomics: piped stdin becomes context next to a
  positional instruction, or the prompt itself when no instruction is given.
- Added `--project <dir>` project intelligence with a bounded file index and
  source/config slice.
- Added `skifflm model list/info/install/remove`, `skifflm git
  diff/review/explain/commit/log/status`, and `skifflm --doctor --network`.
- Added `scripts/install.sh`, `docs/GOOD_FIRST_ISSUES.md`, and removed the
  duplicate lowercase `docs/architecture.md`.
- Added `skifflm session list/show/use/remove`, persistent local memory
  (`--remember`, `--forget`, `/remember`, `/forget`, `/memories`,
  `/clear-memories`), and a `--summarize <file>` shortcut.
- Made the local server concurrent: fast endpoints answer while a chat
  generation is running; generation is serialized behind a mutex so a single
  llama.cpp context stays safe.
- Added `/compact` for compressing long conversations.
- Android: share intent intake, quick prompt chips, and persistent facts.
- Added safe `--code` mode that proposes unified diffs without editing files,
  and made `scripts/release.sh` emit `checksums.txt` plus an optional Android
  APK.
- Added an honest `docs/demo.md` transcript and set up repository labels for
  good-first-issue work.
- Added free-storage checks before Android model downloads.
- Persist Android load, sampling, and system-prompt settings across restarts.

### Mobile parity

- Android now passes user-defined stop sequences through the JNI bridge into
  `GenerationOptions.stop_sequences`, matching the desktop CLI and iOS client.
- Android and iOS add sampling profile presets
  (balanced/fast/creative/code/precise) that apply immediately and remain
  editable.
- Android and iOS add conversation compaction (`Compact conversation` /
  `compactConversation()`), mirroring the desktop `/compact` command with the
  same compression prompt, temperature `0.2`, disabled stop sequences, and
  preserved bullet-summary history.
- Android and iOS header/model info now include model parameter count and
  training context in addition to the native model description, so the three
  clients display the same useful model metadata.
- Android and iOS can attach a text/JSON/XML file from the system picker,
  read it locally as UTF-8 (capped at 64 KB), and include it in the next user
  message, mirroring the desktop `--attach`/`/file` workflow. Attached content
  never leaves the device.
- Android and iOS add a persisted `Code mode` toggle that mirrors the desktop
  safe `--code` behavior: models propose concrete unified diffs without
  claiming files were changed, and applying the proposal stays manual.
- Android model downloads now verify the expected catalog size, validate the
  GGUF header, and write/delete a `.gguf.sha256` sidecar, so both mobile
  clients (and `skifflm model verify`) use the same local integrity convention
  as the desktop CLI.
- Android and iOS add a re-run warm-up action (`Warm up model`), a 3-round
  measured benchmark (`Run 3-round benchmark`) matching the desktop
  `--benchmark` flow, and a live session counter replacing the desktop
  `/stats` behavior on devices. All numbers come from real inference.
- The Android JNI bridge now exposes warm-up as a public engine action, so the
  same native warm-up pass used at load time can be triggered on demand.
- Android and iOS add a one-tap copy action for the most recent assistant
  answer (Android also shows a toast), matching the desktop clipboard-friendly
  one-shot workflow.
- Importing a GGUF from the device now verifies the GGUF header before
  loading on both Android and iOS, matching the desktop `model verify` rule and
  rejecting arbitrary or truncated files.
- Added `/regenerate` (alias `/retry`) to the desktop interactive shell and a
  Regenerate action to Android and iOS. All three clients remove the previous
  assistant response and re-run the last user message with the current
  sampling settings.
- Added `skifflm session rename <old> <new>` on desktop and conversation rename
  on Android and iOS, so all three clients can keep named conversations tidy
  instead of only creating and deleting them.
- Android and iOS conversations now store their own sampling settings and
  code-mode flag inside the local conversation JSON, so switching conversations
  restores that conversation's exact setup instead of relying on global
  defaults.
- iOS and Android READMEs document the shared settings surface; the Android
  app exposes stop sequences, profiles, and compaction from its Settings
  dialog.
- The Android model info JSON path (`EngineController.modelDescription()`)
  now surfaces `description`, `params`, and `context_train`.

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
