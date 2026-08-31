# Contributing

First of all, thank you for helping to make SkiffLLM better.

This repository is intentionally small and easy to review. Please keep it that way.

## Ground Rules

- Open an issue before making a large or design-level change.
- Keep the public interface and documentation in English.
- Do not add telemetry, analytics, network calls at runtime, or cloud dependencies.
- Do not add comment litter to the public source.
- Add tests for any behavior that can be tested without a model file.
- Keep the project buildable with a standard C++17 compiler.

## Development Setup

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
ctest --test-dir build --output-on-failure
```

If you have an existing llama.cpp checkout, use it to avoid downloading:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DSKIFFLLM_FETCH_LLAMA=OFF
```

## Branch Strategy

For each contribution, create a dedicated branch from `main`:

```bash
git checkout -b feature/something
git push origin feature/something
```

Then open a pull request into `main`.

## Pull Request Checklist

- One logical change per pull request.
- Build succeeds.
- `ctest` passes.
- CLI help and README are updated when needed.
- CHANGELOG has a short entry.
- No unrelated files are touched.

## Testing

Unit tests should not require a model. They validate configuration parsing,
argument parsing, profiles, and session persistence.

Model-dependent behavior is checked manually with a small GGUF model. When
adding engine-related behavior, describe how to reproduce with a known model.

## Code Style

- C++17.
- 4-space indentation.
- 100-column limit.
- `snake_case` for functions and variables.
- `PascalCase` for types.
- No `using namespace std;` in headers.
- Prefer `std::string_view` where it avoids copies in hot paths.

## License

By contributing, you agree that your contributions are licensed under the MIT
License.
