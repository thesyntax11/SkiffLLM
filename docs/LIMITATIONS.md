# Known Limitations

This list is intentionally honest. It describes the current state of the
project.

## Model files

SkiffLLM does not bundle models. The desktop runtime requires a GGUF file to
already exist, which keeps its own runtime network use to zero and raises setup
cost compared with services that include retrieval.

Two explicit options exist:

- Desktop: `python3 scripts/model_fetch.py --model <id>`
- Android: `Settings` → `Models` → `Download`

Both are user-initiated HTTPS transfers from Hugging Face. The Android app uses
`INTERNET` permission only for this purpose.

## Local API server

The `--serve` mode is a compact, local HTTP server. It is not a multi-threaded
production gateway.

- Fast endpoints (`/health`, `/version`, `/v1/models`) answer while a chat
  generation is running; generation is serialized behind a mutex because a
  llama.cpp context is not thread-safe.
- `/v1/*` endpoints accept an optional `--api-key` and then require
  `Authorization: Bearer <key>`. They are public when no key is set, which is
  safe only when listening on `127.0.0.1`.
- It has no rate limiting.
- It is intended for local tooling, editor plugins, and personal automation.

## Context

Generation is bounded by the configured context size. Very long conversations
are trimmed only when `--auto-trim` is enabled; otherwise an error is produced
when the prompt is too large.

## Prompt templates

The chat template override accepts a name supported by the loaded llama.cpp
model. Unknown template names fall back to `chatml` or fail with a clear error.

## Android

- Runs one generation at a time and one download at a time.
- The app warms and loads models on the UI-ish background executor; large
  models still take time and memory.
- Model downloads require enough free storage and a network connection.
- GPU offload depends on llama.cpp build support and the device backend.

## CI

The CI and release workflow files are present in the working tree but are not
pushed to this branch until the repository grants the required `workflows`
permission. Until then, local checks can be run with `scripts/ci-local.sh`
(desktop) and `scripts/ci-android.sh` (Android, with the Android SDK
installed).

## Planned

- Embeddings and retrieval-augmented generation
- Multi-worker generation across multiple models
- Package manager integration
- Grammar-constrained generation
- Backend benchmark matrix
