# SkiffLLM

<p align="center">
  <strong>An offline-first local LLM terminal assistant built on llama.cpp.</strong><br/>
  No cloud. No API keys. No accounts. No telemetry. No recurring costs.
</p>

<p align="center">
  <a href="https://github.com/thesyntax11/SkiffLLM/actions">
    <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI status"/>
  </a>
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/badge/c++-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey" alt="Platforms"/>
</p>

SkiffLLM is a fast, privacy-first terminal assistant that runs large language models
entirely on your machine. It is built in modern C++17 and uses llama.cpp for inference,
so it works with any GGUF model, on the CPU or your GPU, with no internet connection.

## Why SkiffLLM

- **Truly offline.** Prompts and answers never leave your computer.
- **Zero cost.** No tokens, no subscriptions, no API costs.
- **Fast.** Direct llama.cpp integration with CPU, CUDA, Metal, and other backends.
- **Private by design.** No telemetry, no crash reporters, no cloud endpoints.
- **Scriptable.** JSON output, stdin pipelines, prompt files, and output files.
- **Persistent.** Conversation state survives restarts.
- **Cross-platform.** Linux, macOS, and Windows through CMake.
- **Extensible.** Small, readable core library with a clean public header surface.

## Highlights

| Capability | Description |
| --- | --- |
| Interactive shell | Streaming token output with slash commands |
| Named sessions | Keep separate conversations, even across different models |
| Model discovery | Automatically find GGUF files in your model directory |
| Sampling profiles | `balanced`, `fast`, `creative`, `code`, `precise` |
| Advanced sampling | temperature, top-p, top-k, min-p, typical-p, penalties |
| Context management | automatic trimming and reserved generation space |
| Stop sequences | terminator strings that halt generation on demand |
| JSON mode | machine-readable output for scripts and tooling |
| Output file | save generated answers to disk |
| Config file | key/value configuration with CLI override priority |
| Environment vars | `SKIFFLLM_*` variables for reproducible workflows |
| GPU offload | offload layers to CUDA, Metal, ROCm, SYCL, Vulkan |
| Shell completions | bash, zsh, and fish completion files |
| CI | GitHub Actions matrix and tests |

## Requirements

- A C++17 compiler
  - GCC 10 or newer
  - Clang 12 or newer
  - MSVC 2019 or newer
- CMake 3.20 or newer
- Optional: CMake's `FetchContent` to download a pinned llama.cpp
- A GGUF model file
- Optional: a CUDA or Metal GPU for hardware acceleration

SkiffLLM has no runtime dependencies beyond your model and llama.cpp.

## Quick Start

### 1. Build

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

### 2. Get a small GGUF model

Place it in the model directory:

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/my-model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

or pass the path directly.

Good starting points for a small, fast, fully offline setup:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

### 3. Chat

```bash
./build/skifflm --model ~/models/my-model-q4_k_m.gguf
```

### 4. Script it

```bash
./build/skifflm --model model.gguf --prompt "Write a short poem."
echo "Summarize this" | ./build/skifflm --model model.gguf
./build/skifflm --model model.gguf --prompt-file prompt.txt --output answer.md
./build/skifflm --model model.gguf --prompt "Explain." --json
```

## Command Line

```text
SkiffLLM - An offline-first local LLM terminal assistant built on llama.cpp

Usage: skifflm [options] [model.gguf]

Core options:
  --model <path>             Path to a GGUF model file
  --model-dir <path>         Directory scanned for a GGUF model
  --list-models              Print discovered GGUF models
  --profile <name>           balanced, fast, creative, code or precise
  --session <name>           Use a named conversation
  --system <text>            System prompt
  --stop <text>              Stop sequence; can be repeated

Inference options:
  --ctx <n>                  Context size (default: 4096)
  --batch <n>                Batch size (default: 512)
  --ubatch <n>               Physical batch size (default: auto)
  --threads <n>              CPU threads (0 means auto)
  --gpu-layers <n>           GPU layers to offload (-1 means all)
  --mmap                     Enable mmap (default)
  --no-mmap                  Disable mmap
  --mlock                    Lock model memory

Sampling options:
  --temp <t>                 Sampling temperature (default: 0.70)
  --top-p <p>                Nucleus sampling (default: 0.95)
  --top-k <n>                Top-K sampling (default: 40)
  --min-p <p>                Minimum probability filter
  --typical <p>              Locally typical sampling
  --repeat-penalty <p>       Repeat penalty (default: 1.10)
  --repeat-last-n <n>        Repeat penalty window (default: 64)
  --n-predict <n>            Max generated tokens (default: 512)
  --seed <n|random>          Sampling seed (default: random)

Session options:
  --history <path>           Session history file
  --reset-history            Ignore and overwrite saved history
  --no-save                  Do not persist the session
  --auto-trim                Trim old history when context is full (default)
  --no-auto-trim             Fail instead of trimming
  --reserve-ctx <n>          Reserve tokens for generation

Program options:
  --prompt <text>            Single prompt mode
  --prompt-file <path>       Read the single prompt from a file
  --stdin                    Read the single prompt from stdin
  --json                     Machine-readable JSON output
  --output <path>            Write the text answer to a file
  --non-interactive          Disable the interactive shell
  --color                    Force ANSI colors (default: auto)
  --no-color                 Disable ANSI colors
  --no-banner                Hide the startup banner
  --verbose                  Print llama.cpp logs
  --config <path>            Config file path
  --show-config              Print the effective configuration
  --help                     Show this help
  --version                  Show the version

Interactive commands: /help /info /history /clear /reset /system /model /temp /top-p /top-k /n /save /exit
```

## Interactive Commands

```text
/help                 Show the command list
/info                 Show model and session information
/history              Show the current conversation
/clear                Clear the conversation history
/reset                Clear history and restore the default system prompt
/system <text>        Set or show the system prompt
/model <path>         Reload the model from a GGUF file
/temp <value>         Change sampling temperature
/top-p <value>        Change nucleus sampling threshold
/top-k <value>        Change top-k sampling value
/n <value>            Change maximum generated tokens
/save                 Save the session history now
/exit or /quit        Exit SkiffLLM
```

Any line that is not a command is sent to the model.

## Configuration File

The default config location is:

```text
~/.config/skifflm/config
```

A complete example is available at `configs/skifflm.example.conf`:

```text
model=/home/user/models/model.gguf
model-dir=/home/user/models
session=main
profile=balanced
system=You are a helpful local assistant.

ctx=4096
batch=512
ubatch=256
threads=8
gpu-layers=0

temp=0.70
top-p=0.95
top-k=40
min-p=0.0
typical=0.0
repeat-penalty=1.10
repeat-last-n=64
n-predict=512

stop=END
stop=STOP
auto-trim=yes
reserve-ctx=128
save-history=yes
```

Command line arguments take priority over the config file.

## Environment Variables

```text
SKIFFLLM_MODEL       Default model path
SKIFFLLM_MODEL_DIR   Default model directory
SKIFFLLM_CONFIG      Config file path
SKIFFLLM_HISTORY     History file path
SKIFFLLM_SYSTEM      Default system prompt
SKIFFLLM_PROFILE     Default sampling profile
```

## JSON Output

`--json` prints a single machine-readable object with the generated answer and
timing information. It is ideal for scripts, plugins, and other tools.

```bash
skifflm --model model.gguf --prompt "List three fruits." --json
```

```json
{
  "text": "1. Apple\n2. Banana\n3. Cherry\n",
  "model": "/home/user/models/model.gguf",
  "prompt_tokens": 12,
  "generated_tokens": 42,
  "prompt_ms": 123.4,
  "generation_ms": 2400.1,
  "tokens_per_second": 17.49,
  "stopped": false
}
```

## Session Files and Named Sessions

The default history file is:

```text
~/.local/share/skifflm/history.skif
```

Use `--session name` to keep independent conversations:

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

The system prompt is persisted with the conversation.

## Profiles

| Profile | Temperature | Top-p | Top-k | Min-p | Repeat | Max tokens | Suggested usage |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `balanced` | 0.70 | 0.95 | 40 | 0.00 | 1.10 | 512 | General assistant |
| `fast` | 0.60 | 0.90 | 30 | 0.00 | 1.05 | 256 | Quick answers |
| `creative` | 1.00 | 0.98 | 60 | 0.05 | 1.20 | 512 | Writing and ideas |
| `code` | 0.20 | 0.90 | 20 | 0.00 | 1.20 | 1024 | Code generation |
| `precise` | 0.00 | 0.90 | 20 | 0.00 | 1.00 | 650 | Deterministic tasks |

## CMake Presets

A preset-based build is the simplest path when Ninja is available:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Available presets: `release` and `debug`.

## Build Options

| Option | Default | Description |
| --- | --- | --- |
| `SKIFFLLM_BUILD_TESTS` | `ON` | Build and register the test suite |
| `SKIFFLLM_FETCH_LLAMA` | `ON` | Download and build a pinned llama.cpp |
| `SKIFFLLM_LLAMA_SOURCE_DIR` | empty | Use an existing llama.cpp checkout |
| `SKIFFLLM_BUILD_SHARED_LLAMA` | `OFF` | Build llama.cpp as a shared library |

### Build with an existing llama.cpp checkout

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DSKIFFLLM_FETCH_LLAMA=OFF
```

### Build without downloading llama.cpp at configure time

```bash
cmake -S . -B build -DSKIFFLLM_FETCH_LLAMA=OFF
```

This requires a system llama package that CMake can find.

### Build and run a specific backend

llama.cpp detects the available backends. For CUDA:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON
```

For Metal on macOS, the Metal backend is enabled automatically.

## Install

```bash
cmake --install build --prefix /usr/local
```

The binary is installed as `skifflm`.

## Shell Completions

### bash

```bash
source scripts/completions/skifflm.bash
```

### zsh

```bash
cp scripts/completions/skifflm.zsh /usr/local/share/zsh/site-functions/
```

### fish

```bash
cp scripts/completions/skifflm.fish ~/.config/fish/completions/
```

## Testing

```bash
ctest --test-dir build --output-on-failure
```

The suite covers configuration parsing, argument parsing, profiles, session
round-tripping, and the command-line entry points.

## Performance

SkiffLLM does not add a proxy layer. Tokens go directly through llama.cpp.
Generation speed is determined by your hardware, model size, quantization, and
GPU offload.

Suggested speedups:

- Use the largest context your session actually needs.
- Use `--profile fast` for quick single-answer prompts.
- Use `--n-predict` to bound generation.
- Offload layers with `--gpu-layers`.
- Keep the session small when context is limited; SkiffLLM can auto-trim old
  turns when context fills up.
- Use a quantized model such as `Q4_K_M` for the best speed/quality balance.

## Offline Guarantee

- No network access used at runtime.
- No remote inference endpoints.
- No API keys or accounts.
- No telemetry, analytics, or crash reporting.
- Prompts and outputs stay on the local machine.

The only optional network use is during the build when CMake downloads the
pinned llama.cpp source tree. Supply an existing checkout with
`SKIFFLLM_LLAMA_SOURCE_DIR` to avoid it completely.

## Project Layout

```text
CMakeLists.txt                Build configuration
Licenses / README             Distribution metadata
include/skifflm/              Public API headers
src/                          CLI and core implementation
tests/                        Unit and integration tests
configs/                      Example configuration
scripts/                      Build helper, benchmark helper and shell completions
.github/workflows/            CI workflow
```

## Roadmap

- Readline-based line editing with arrow-key navigation
- Token-level streaming stats in the interactive UI
- Embedding and RAG mode
- System dependency packaging (Homebrew, apt, vcpkg)
- GPU benchmark matrix
- Grammar-constrained generation
- Model download/quantization helper

## Contributing

Contributions are welcome. Please:

1. Open an issue for major features before submitting a pull request.
2. Keep changes small and reviewable.
3. Add tests for new behavior.
4. Keep the interface English and free of comments in the public source.

## License

SkiffLLM is released under the [MIT License](LICENSE).
