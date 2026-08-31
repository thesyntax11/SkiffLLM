# Known Limitations

This list is intentionally honest. It describes the current state of the
project.

## Model files

SkiffLLM does not bundle or download models. A GGUF file must already exist on
the machine. This keeps runtime network use to zero, but it raises the setup
cost compared with services that include model retrieval.

## Local API server

The `--serve` mode is a compact, local, single-request HTTP server. It is not
a multi-threaded production gateway.

- It handles one request at a time.
- It has no authentication.
- It has no rate limiting.
- It is intended for local tooling, editor plugins, and personal automation.

## Context

Generation is bounded by the configured context size. Very long conversations
are trimmed only when `--auto-trim` is enabled; otherwise an error is produced
when the prompt is too large.

## Prompt templates

The chat template override accepts a name supported by the loaded llama.cpp
model. Unknown template names fall back to `chatml` or fail with a clear error.

## CI

The CI and release workflow files are present in the working tree but are not
pushed to this branch until the repository grants the required `workflows`
permission. Until then, local checks can be run with `scripts/ci-local.sh`.

## Planned

- Embeddings and retrieval-augmented generation
- Concurrent server requests
- Package manager integration
- Grammar-constrained generation
- Backend benchmark matrix
