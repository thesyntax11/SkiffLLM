# Contributing

Thanks for helping improve SkiffLLM. This project is small and intentionally
simple, so clean, honest contributions are the most valuable.

## Development setup

### Desktop

```bash
make release
make tests
```

Or use the underlying CMake/CTest commands directly:

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

Use `scripts/ci-local.sh` or `make check` for the full local CI check:

```bash
make check
bash scripts/ci-local.sh
```

### Android

Open `android/` in Android Studio 2024.1+ and sync the Gradle project, or build
from the CLI:

```bash
cd android
./gradlew assembleDebug
```

The native build downloads the pinned llama.cpp revision unless you pass
`-Pskifflm.llamaSourceDir=/path/to/llama.cpp`.

For a scripted Android check with the SDK installed:

```bash
SKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp bash scripts/ci-android.sh
```

## What is expected

- Keep the honest-measurement rule. Never invent benchmarks, token counts, or
  speed numbers.
- Do not add runtime telemetry, analytics, crash reporting, or cloud APIs to
  the desktop runtime.
- The Android `INTERNET` permission is only justified by model downloads.
- Keep the runtime offline. Scripts may fetch models, but the application
  should not.
- Run `python3 -m py_compile scripts/*.py` after changing Python helpers.
- Format C++ with `clang-format` if available.
- Add changelog entries under `CHANGELOG.md` for user-visible changes.

## Commit style

Use focused commits with imperative subjects:

- `feat(desktop): ...`
- `fix(android): ...`
- `docs: ...`
- `ci: ...`
- `chore: ...`

## Testing

- CTest unit tests run without a model and are required to pass.
- `scripts/model_fetch.py --list` and `scripts/api_client.py --help` must work.
- Feature changes should avoid requiring a real GGUF model in the default test
  suite unless there is an explicit `SKIFFLLM_TEST_MODEL` opt-in.
