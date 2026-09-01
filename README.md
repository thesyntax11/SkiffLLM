# SkiffLLM

<p align="center">
  <strong>An offline-first local LLM terminal assistant built on llama.cpp.</strong><br/>
  No cloud. No API keys. No accounts. No telemetry. No recurring costs.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android-lightgrey" alt="Platforms"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Offline"/>
</p>

SkiffLLM is a fast, privacy-first terminal assistant that runs large language models
entirely on your machine. It is built in modern C++17 and uses llama.cpp for inference,
so it works with any GGUF model, on the CPU or your GPU, with no internet connection.

Documentation: [Setup](docs/SETUP.md) &middot; [Usage](docs/usage.md) &middot; [Architecture](docs/ARCHITECTURE.md) &middot; [Releasing](docs/RELEASING.md) &middot; [Limitations](docs/LIMITATIONS.md) &middot; [Security](SECURITY.md) &middot; [Contributing](CONTRIBUTING.md)

## Why SkiffLLM

- **Truly offline.** Prompts and answers never leave your computer.
- **Zero cost.** No tokens, no subscriptions, no API costs.
- **Fast.** Direct llama.cpp integration with CPU, CUDA, Metal, and other backends.
- **Private by design.** No telemetry, no crash reporters, no cloud endpoints.
- **Scriptable.** JSON output, stdin pipelines, prompt files, and output files.
- **Persistent.** Conversation state survives restarts.
- **Cross-platform.** Linux, macOS, and Windows through CMake.
- **Extensible.** Small, readable core library with a clean public header surface.

## How SkiffLLM differs

- **llama.cpp** is the inference engine and toolkit. SkiffLLM is a focused
  assistant experience on top of it: named sessions, file context, export,
  profiles, streaming UX, benchmarks, and a small local API.
- **Ollama** is a model manager and runtime with a registry. SkiffLLM is a
  single binary that operates directly on a GGUF file you already own. Its
  runtime does not download, pull, or register models, and it does not run a
  daemon. An optional `scripts/model_fetch.py` helper can fetch a recommended
  GGUF for you once; inference itself stays fully offline.
- **A cloud API** sends prompts to a remote service. SkiffLLM keeps every token
  on the machine at runtime.

## Highlights

| Capability | Description |
| --- | --- |
| Interactive shell | Streaming token output, history, and live token counters |
| File context | `--attach`, `/file`, and `@file` expansion in any prompt |
| Conversation export | `--export` and `/export` save sessions as Markdown |
| Chat template override | `--chat-template` for models with custom prompt formats |
| Model warmup | `--warmup` reduces first-answer latency |
| Local API server | `--serve` exposes a local OpenAI-compatible endpoint |
| Real benchmark | `--benchmark <runs>` measures actual prompt and generation speed |
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
| Flash attention | opt-in `--flash-attn` with KV offload control |
| NUMA support | `--numa` for multi-socket systems |
| Diagnostics | `--doctor` system report and `--model-info` metadata |
| Tokenizer inspection | `--tokenize` and `/tokenize` |
| Smoke test | `--smoke` for a quick end-to-end generation check |
| Shell completions | bash, zsh, and fish completion files |
| Local checks | `scripts/ci-local.sh` runs build, tests, and entry points |

## Android

A native Android client is included under [`android/`](android/README.md). It runs
supported GGUF models entirely on the phone with Jetpack Compose and a llama.cpp
JNI bridge. Tokens stream into a dark chat UI, and every response shows real
measured token count, time, and tokens per second. The app remembers the last
loaded model, and it can download recommended GGUF models directly from Hugging
Face over HTTPS. The only network use is those downloads; prompts and history
never leave the device.

See the [Android model guide](android/models.md) for the recommended small,
quantized models for phones.

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

Option A — fetch a recommended model:

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Files are saved to `~/.local/share/skifflm/models` by default.

Option B — use an existing file:

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/my-model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

or pass the path directly with `--model`.

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
./build/skifflm --model model.gguf --attach paper.txt --prompt "Summarize this file."
./build/skifflm --model model.gguf --prompt "Read @TODO.md and list the tasks."
./build/skifflm --model model.gguf --warmup --prompt "Say hello."
./build/skifflm --export conversation.md
./build/skifflm --model model.gguf --benchmark 3
./build/skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080
```

## Command Line

```text
SkiffLLM - An offline-first local LLM terminal assistant built on llama.cpp

Usage: skifflm [options] [model.gguf]

Core options:
  --model <path>             Path to a GGUF model file
  --model-dir <path>         Directory scanned for a GGUF model
  --list-models              Print discovered GGUF models
  --model-info               Print model metadata and exit
  --smoke                    Run a quick generation smoke test
  --warmup                   Warm the model before the first answer
  --doctor                   Print system diagnostics
  --tokenize <text>          Tokenize text and print token counts
  --profile <name>           balanced, fast, creative, code or precise
  --session <name>           Use a named conversation
  --system <text>            System prompt
  --stop <text>              Stop sequence; can be repeated
  --attach <path>            Attach a file; can be repeated
  --file <path>             Alias for --attach; can be repeated
  --chat-template <name>     Override the model chat template
  --export <path>            Export the loaded conversation as Markdown
  --serve                    Serve a local OpenAI-compatible API
  --host <addr>              Local server bind address (default: 127.0.0.1)
  --port <n>                 Local server port (default: 8080)
  --benchmark <runs>         Run a real generation benchmark

Inference options:
  --ctx <n>                  Context size (default: 4096)
  --batch <n>                Batch size (default: 512)
  --ubatch <n>               Physical batch size (default: auto)
  --threads <n>              CPU threads (0 means auto)
  --gpu-layers <n>           GPU layers to offload (-1 means all)
  --mmap                     Enable mmap (default)
  --no-mmap                  Disable mmap
  --mlock                    Lock model memory
  --flash-attn               Enable flash attention
  --no-flash-attn            Disable flash attention
  --numa                     Initialize NUMA optimization
  --kv-offload               Offload KV cache to device (default)
  --no-kv-offload            Keep KV cache on CPU

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
  --n-keep <n>               Keep at least n turns during trimming

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
  --quiet                    Suppress banners and llama logs
  --verbose                  Print llama.cpp logs
  --config <path>            Config file path
  --show-config              Print the effective configuration
  --help                     Show this help
  --version                  Show the version

Interactive commands: /help /info /warmup /history /settings /tokenize /file /clear-attach /clear /reset /system /model /profile /stop /temp /top-p /top-k /min-p /typical /n /ctx /export /save /exit
```

## Interactive Commands

```text
/help                 Show the command list
/info                 Show model and session information
/warmup               Run a short warm-up pass
/history              Show the current conversation
/export <path>        Export the conversation as Markdown
/settings             Show the current sampling settings
/tokenize <text>      Show the token count for the text
/clear                Clear the conversation history
/reset                Clear history and restore the default system prompt
/system <text>        Set or show the system prompt
/model <path>         Reload the model from a GGUF file
/file <path>          Attach a file for upcoming messages
/clear-attach         Remove all attached files
/profile <name>       Use balanced, fast, creative, code or precise
/stop <text>          Add a stop sequence; /stop shows current stops
/temp <value>         Change sampling temperature
/top-p <value>        Change nucleus sampling threshold
/top-k <value>        Change top-k sampling value
/min-p <value>        Change minimum probability filter
/typical <value>      Change locally typical sampling value
/n <value>            Change maximum generated tokens
/ctx <value>          Change the context size
/save                 Save the session history now
/exit or /quit        Exit SkiffLLM
```

Any line that is not a command is sent to the model.

## File Context and Conversation Export

Attach files for the next message in one-shot mode, or until cleared in
interactive mode:

```bash
skifflm --model model.gguf --attach report.txt --prompt "Summarize the report."
```

Attach multiple files by repeating `--attach` or by writing `@path` directly in the prompt:

```bash
skifflm --model model.gguf --attach report.txt --attach data.csv \
  --prompt "Compare @report.txt with @data.csv."
```

Interactive mode uses the same commands:

```text
/file report.txt      Attach a file to the next message
/clear-attach         Remove all attached files
```

Attached content is wrapped in a small file tag so the model can tell where the
file begins and ends. The file content is read locally and is never uploaded.

Export any conversation as Markdown without loading a model:

```bash
skifflm --export conversation.md
```

Or export the current conversation from inside the shell:

```text
/export conversation.md
```

## Local API Server

SkiffLLM can serve a local, offline OpenAI-compatible HTTP endpoint:

```bash
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080
```

Endpoints:

```text
GET  /health                 Simple health check
GET  /v1/models              List the single loaded model
POST /v1/chat/completions    Chat completion, with OpenAI-style streaming support
```

Example request:

```bash
curl http://127.0.0.1:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "model.gguf",
    "messages": [{"role": "user", "content": "Say hello."}],
    "stream": false
  }'
```

A small dependency-free Python client is included:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --stream "Tell me a short joke."
python3 scripts/api_client.py http://127.0.0.1:8080 --prompt "Hello" --temperature 0.7
```

The server binds to `127.0.0.1` by default. It responds to CORS preflight
(`OPTIONS`) and sends CORS headers on normal and streaming responses, so
browser-based clients can use it locally. It has no auth and no remote
exposure; if you bind to `0.0.0.0`, you are responsible for the network
security of that interface. It is intentionally simple and handles one
request at a time.

## Benchmark

`--benchmark <runs>` runs a real generation on the loaded model and reports the
actual measured timing. It never fabricates numbers:

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Defaults to 128 tokens per run and a fixed prompt; use `--n-predict` to change
the run length. `--json` emits machine-readable run data and averages.

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
chat-template=chatml
warmup=yes
attach=notes.txt
export=last-conversation.md
serve=no
host=127.0.0.1
port=8080
benchmark=0

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
| `SKIFFLLM_USE_READLINE` | `ON` | Enable GNU Readline editing and history when available |

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
scripts/ci-local.sh
```

The suite covers configuration parsing, argument parsing, profiles, session
round-tripping, and the command-line entry points. `scripts/ci-local.sh`
runs the full local CI path without GitHub.

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
include/skifflm/              Public API headers
src/                          CLI, core, and local server implementation
tests/                        Unit and integration tests
configs/                      Example configuration
scripts/                      Build helper, benchmark helper and shell completions
.github/workflows/            CI workflow
```

## Roadmap

- Embedding and RAG mode
- System dependency packaging (Homebrew, apt, vcpkg)
- GPU benchmark matrix across backends
- Grammar-constrained generation
- Model download/quantization helper
- Token-level sampling diagnostics
- Concurrent request handling for the local server

## Known Limitations

SkiffLLM does not bundle models, the local server is single-request and has no
auth, and retrieval-augmented generation is not implemented yet. See
[Known Limitations](docs/LIMITATIONS.md) for an honest list.

## Contributing

Contributions are welcome. Please:

1. Open an issue for major features before submitting a pull request.
2. Keep changes small and reviewable.
3. Add tests for new behavior.
4. Keep the interface English and free of comments in the public source.

## License

SkiffLLM is released under the [MIT License](LICENSE).
