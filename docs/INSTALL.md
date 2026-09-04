# Installing SkiffLLM

SkiffLLM is a single static/small C++ binary that talks to llama.cpp. The
runtime does not bundle a model, does not call the network, and does not need a
daemon.

## 1. Install a prebuilt archive

When a release is published it includes platform archives named
`skiffllm-<version>-<os>-<arch>.tar.gz` (or `.zip` on Windows), standalone
native binaries, Android APK files, an iOS app container, and
`checksums.txt`.

```bash
# Linux / macOS
tar -xzf skiffllm-v1.9.0-linux-x86_64.tar.gz
sudo install -m 0755 bin/skiffllm /usr/local/bin/skiffllm
```

Standalone executables follow `skiffllm-<version>-<os>-<arch>[.exe]`. The Windows
asset is available both as a zip and as a direct `.exe`. The iOS asset is an
`.ipa` app container; a device install requires signing with an Apple
development identity.

A ready-to-run helper is included:

```bash
bash scripts/install-from-release.sh --version v1.9.0
bash scripts/install-from-release.sh --help
```

The helper maps the current OS/arch to the release asset and installs to
`$HOME/.local/bin`. It fails fast when the archive is not published yet.

## 2. Install the latest release

```bash
curl -fsSL https://raw.githubusercontent.com/thesyntax11/SkiffLLM/main/scripts/install-latest.sh | bash
```

## 3. Install from a source checkout

### Quick local install

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

Useful flags:

```bash
bash scripts/install.sh --help                 # all options
bash scripts/install.sh --backend vulkan        # GPU backend build
bash scripts/install.sh --skip-tests            # build only
```

### CMake install

```bash
cmake --preset release
cmake --build build/release -j
cmake --install build/release --prefix /usr/local
```

## 4. Manual build

Requirements:

- CMake 3.20+
- A C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- Ninja (recommended) or Make
- Python 3 only needed for the optional model-fetch helper

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If you already have a llama.cpp checkout built and installed:

```bash
cmake -S . -B build \
  -DSKIFFLLM_FETCH_LLAMA=OFF \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DCMAKE_BUILD_TYPE=Release
```

## 5. Get a model

The runtime never downloads models.

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Files land in `~/.local/share/skiffllm/models` by default. You can also point
`--model` at any existing `.gguf`.

## 6. Shell completions

Source or copy the generated files:

```bash
# bash
source scripts/completions/skiffllm.bash

# zsh
cp scripts/completions/skiffllm.zsh ~/.zsh_functions/

# fish (completions directory)
cp scripts/completions/skiffllm.fish ~/.config/fish/completions/
```

## 7. macOS / iOS

- macOS builds through the same CMake path; use `--backend metal` for Metal.
- iOS requires macOS/Xcode + XcodeGen: `bash scripts/ios-setup.sh`.

## 8. Android

- Android Studio is not on this install path.
- `bash scripts/ci-android.sh` produces a debug APK when the Android SDK is
  installed.

## 9. Record a demo

Run `scripts/demo-capture.sh` with a real binary and a real GGUF model to
produce an honest terminal recording. The script never creates fake output.

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /path/model.gguf
```

