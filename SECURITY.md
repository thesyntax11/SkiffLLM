# Security Policy

SkiffLLM is designed to run fully offline. Generated content should be treated
like output from any local language model.

## Supported Versions

The current `main` branch is the only actively supported version.

## Reporting a Vulnerability

Please do not open a public issue for a security problem. Instead, contact the
maintainers privately through the repository security advisory workflow on
GitHub.

When reporting, please include:

- The version or commit you tested.
- The operating system and hardware configuration.
- A minimal reproducible example.
- The impact you observed.
- A suggested fix, if available.

## Security Model

- Runtime inference is fully offline.
- No telemetry or analytics is included.
- No API keys are stored or transmitted.
- Session files are local.
- The build may download llama.cpp only when CMake FetchContent is used.
- Users should place their own GGUF models in a trusted local directory.
