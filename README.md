## SkiffLLM

<p align="center">
  <strong>A fast, private, offline AI engine for your terminal.</strong><br/>
  Pipe anything into a local LLM. No cloud. No API keys. No telemetry.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/badge/version-v1.6.0-blue" alt="Version v1.6.0"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI status"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Platforms"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Offline"/>
</p>

SkiffLLM is a single, local AI runtime that feels like a Unix tool. It runs any
GGUF model through llama.cpp on your CPU or GPU, keeps every token on your
machine, and drops straight into your shell workflow.

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

Documentation: [Setup](docs/SETUP.md) &middot; [Usage](docs/usage.md) &middot; [Demo](docs/demo.md) &middot; [Architecture](docs/ARCHITECTURE.md) &middot; [Releasing](docs/RELEASING.md) &middot; [Limitations](docs/LIMITATIONS.md) &middot; [FAQ](docs/FAQ.md) &middot; [Security](SECURITY.md) &middot; [Contributing](CONTRIBUTING.md) &middot; [Good First Issues](docs/GOOD_FIRST_ISSUES.md)

## Use it anywhere

The killer workflow is the pipe. Feed SkiffLLM a document, a diff, or a log,
then tell it what to do. No file names, no clipboard, no cloud.

```bash
# Review a change set before you push
git diff | skifflm "review these changes"

# Find the real cause in a messy log
journalctl -e | skifflm "find suspicious errors and a likely root cause"

# Summarize a file you just read
cat README.md | skifflm "summarize this"

# Point it at an entire repository
skifflm --project . "where is authentication implemented?"

# Machine-readable output for your own scripts
git diff | skifflm --json "classify this diff"
```

## Why SkiffLLM

| | SkiffLLM | Cloud assistants | Ollama |
| --- | --- | --- | --- |
| Cloud required | ❌ | ✅ | ❌ |
| API key | ❌ | ✅ | ❌ |
| Account / signup | ❌ | ✅ | ❌ |
| Telemetry | ❌ | varies | ❌ |
| Runs a daemon | ❌ | ✅ | ✅ |
| Direct GGUF file | ✅ | ❌ | partial |
| CPU-only machines | ✅ | ❌ | ✅ |
| GPU offload | ✅ | n/a | ✅ |
| Unix pipelines | ✅ | ❌ | ❌ |
| Project/code context | ✅ | ❌ | ❌ |
| Local OpenAI-compatible API | ✅ | n/a | ✅ |
| Native Android client | ✅ | ❌ | ✅ |
| Native iOS client | ✅ | ❌ | ❌ |

SkiffLLM is the lightweight tool you keep at the end of a pipe: no daemon, no
registry, no account. You bring the GGUF file, it brings the inference.

## Highlights

## Highlights

| Capability | Description |
| --- | --- |
| Unix pipelines | `cat file | skifflm "summarize"`, `git diff | skifflm review` |
| Project context | `--project <dir>` adds a real file index + source slice |
| Model manager | `skifflm model list / info / install / remove` |
| Git integration | `skifflm git review / explain / commit / log / status` |
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

## Unix pipeline

SkiffLLM reads stdin automatically, so it composes with everything else in the
shell.

```bash
# Context on stdin, instruction as the argument
git diff | skifflm "review these changes"
cat server.log | skifflm "find errors and suggest a fix"

# Instruction from stdin, structured output
printf "summarize this error log" | skifflm --json

# Feed a file as context without a shell pipe
skifflm "explain this code" < main.cpp

# Attach named files too
skifflm --attach notes.txt --attach README.md "summarize these"
```

`--json` returns `text`, `model`, `prompt_tokens`, `generated_tokens`,
`prompt_ms`, `generation_ms`, `tokens_per_second`, and `stopped` on stdout.

## Project intelligence

Point SkiffLLM at a repository and it builds a bounded, real file index plus a
slice of source/config content before answering.

```bash
skifflm --project . "where is authentication handled?"
skifflm --project src/ "what does the server do?"
```

The block reports total files, source/test/config counts, a file index, and up
to a fixed-size slice of source files. It never walks build caches or `.git`.

### Compacting long conversations

When a session grows too long, `/compact` asks the model to compress the
conversation into a bullet summary while preserving facts and unfinished work,
then replaces the history with that summary.

```bash
/compact
```

### Safe code mode

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` asks for a concrete, reviewable unified diff proposal. It deliberately
does **not** modify files: outputs from a local model need human review, and
applying them automatically is a safety decision the project leaves to you.

## Sessions & persistent memory

Keep separate conversations, and let SkiffLLM remember facts that stay true
across sessions.

```bash
# Separate conversations
skifflm --session coding --model qwen2.5-1.5b.gguf
skifflm --session writing --model qwen2.5-1.5b.gguf

# Manage them
skifflm session list
skifflm session show coding
skifflm session use coding --model qwen2.5-1.5b.gguf
skifflm session rename coding writing
skifflm session remove old-draft
```

Persistent memory is local, plain text, and injected into the active system
prompt.

```bash
skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Inside the interactive shell use `/remember <fact>`, `/forget <text>`,
`/memories`, and `/clear-memories`. Memory lives in
`~/.local/share/skifflm/memories.txt` and never leaves the machine.

## Lovable shortcuts

```bash
# Summary in one command
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf

# Whole project context
skifflm --project . "where is authentication handled?"

# Safe code proposal
skifflm --code --project . "fix the bug in src/server.cpp"
```

## Model manager

SkiffLLM stays offline at runtime. Model retrieval is an explicit, separate
command.

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model remove qwen2.5-0.5b --force
```

`model install` delegates to `scripts/model_fetch.py`, which downloads exactly
one GGUF over HTTPS from Hugging Face, verifies the GGUF header and expected
size, and places it in your model directory. Inference itself never opens a
connection.

## Git integration

Local, offline code review and explanation for the diff in front of you.

```bash
git diff | skifflm git review
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

`git review` returns a prioritized `HIGH`/`MEDIUM`/`LOW` finding list. `git
commit --cached` proposes a conventional commit message from your staged diff;
it does not run `git commit` for you.

## Drop-in OpenAI endpoint

The local server is compatible with the OpenAI client you already have.

```bash
skifflm --model qwen2.5-1.5b.gguf --serve --host 127.0.0.1 --port 8080
# with a shared token:
skifflm --model qwen2.5-1.5b.gguf --serve --host 127.0.0.1 --port 8080 --api-key "local-token"
```

Any API key string is accepted by the OpenAI client; SkiffLLM only compares
the `Authorization: Bearer` value you pass to `--api-key`.

```python
from openai import OpenAI

client = OpenAI(base_url="http://127.0.0.1:8080/v1", api_key="local-token")
resp = client.chat.completions.create(
    model="qwen2.5-1.5b",
    messages=[{"role": "user", "content": "Explain this diff."}],
    stream=True,
)
for chunk in resp:
    print(chunk.choices[0].delta.content or "", end="")
```

The server is concurrent: fast endpoints (`/health`, `/v1/models`,
`/version`) answer while a long `/v1/chat/completions` is generating.
Generation for one model is serialized because llama.cpp contexts are not
thread-safe, so simultaneous chat requests queue instead of corrupting state.

If you bind to `0.0.0.0` or another interface, protect the API with a shared
token:

```bash
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

When `--api-key` is set, `/v1/models` and `/v1/chat/completions` require
`Authorization: Bearer <key>` and return `401` otherwise. `/health`,
`/version`, and `/` stay public so health checks and version probes keep
working. Bind to `127.0.0.1` by default; remote listeners are only useful when
you can protect them.

## Privacy proof

```bash
skifflm --doctor --network
```

prints the runtime facts: no outbound network calls, no telemetry, no cloud
APIs, local history storage, and a `✓ OFFLINE` status. There is no analytics,
crash reporting, or usage tracking in the desktop runtime or the Android app.

## Benchmark honesty

```bash
skifflm --model model.gguf --benchmark 3
```

Every number is measured on your machine with your model and hardware. SkiffLLM
never invents benchmark results, and `CONTRIBUTING.md` asks contributors to
keep it that way.

## One-command install

Builds and prebuilt archives are both supported.

```bash
# From a source checkout
cmake --preset release && cmake --build --preset release -j

# Or the convenience installer
bash scripts/install.sh

# Local archive
bash scripts/release.sh
tar -xzf skifflm-*.tar.gz && ./bin/skifflm --help

# Android development APK (requires the SDK)
bash scripts/ci-android.sh

# iOS development (requires macOS/Xcode + XcodeGen)
bash scripts/ios-setup.sh
```

Prebuilt asset names follow the pattern `skifflm-<version>-<platform>.tar.gz`
with a `checksums.txt`. Release workflow files target Linux/macOS/Windows and
an Android APK.

## Mobile apps

The desktop client has native mobile apps for [iOS](ios/README.md) (SwiftUI +
a llama.cpp Objective-C++ bridge) and [Android](android/README.md) (Jetpack
Compose + a llama.cpp JNI bridge). Both run supported GGUF models entirely on
the phone, stream tokens into a dark/light/system theme, and show a live
context-usage progress bar plus real measured token count, time, and tokens
per second. The apps remember the last loaded model, and can download
recommended GGUF models directly from Hugging Face over HTTPS with size/GGUF
validation and a SHA-256 checksum sidecar. They share the desktop sampling
profiles, stop sequences, safe code mode, multi-conversation management,
persistent facts, quick prompts, conversation compaction, Markdown export,
text-file attachment, warm-up, real 3-round benchmarking, live session
statistics, and share-sheet intake. The only network use is those explicit
downloads; prompts and history never leave the device.

### Android

Android GPU/NPU acceleration is opt-in at build time:

```bash
./gradlew assembleDebug -Pskifflm.backend=vulkan   # or opencl
```

The desktop equivalent is `-DSKIFFLLM_LLAMA_BACKEND=vulkan|opencl`.

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

Or use the CMake presets / convenience targets:

```bash
cmake --preset release
cmake --build --preset release -j
ctest --test-dir build/release --output-on-failure
```

```bash
make preset-release   # configure, build, and test a Release build
make check            # run the full local CI script
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
./build/skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key local-token
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
  --api-key <key>            Require Bearer auth on the local server
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
  --context-bar              Show the live context usage bar (default)
  --no-context-bar           Hide the context usage bar
  --backend-info             Print the active llama.cpp backends and exit
  --help                     Show this help
  --version                  Show the version

Subcommands:
  run [prompt] [opts]        One-shot prompt (e.g. `skifflm run "Merhaba" --ctx 2048 --temp 0.3 --threads 4`)
  model list|info|install|remove|verify
  chat-template list|detect|info
  openai [prompt] [opts]     Send a prompt to a local OpenAI-compatible server
  config path|show|init      Manage the config file
  server health [--json]     Check a running local server

Interactive commands: /help /info /warmup /history /settings /stats /compact /regenerate /tokenize /file /clear-attach /clear /reset /system /model /profile /stop /temp /top-p /top-k /min-p /typical /n /ctx /export /save /exit
```

### One-shot `run`

```bash
skifflm run "Merhaba" --ctx 2048 --temp 0.3 --threads 4
skifflm run "Summarize this file" --attach report.txt --n-predict 256
```

`run` accepts the same inference and sampling flags as any one-shot mode and
streams the answer token-by-token when stdout is a terminal.

### `chat-template`

```bash
skifflm chat-template list                  # known templates
skifflm chat-template detect --model x.gguf # read the template embedded in a model
skifflm model info <id>                     # show catalog + installed model checksum
```

### `model verify`

```bash
skifflm model verify <id>                   # size + GGUF header + SHA-256 sidecar
skifflm model verify <id> --update          # record the local SHA-256 sidecar
python3 scripts/model_fetch.py --model <id> --verify
```

Downloads and locally-recorded checksums are permanent except when the user
deletes the `.gguf.sha256` sidecar. No checksum is ever fabricated.

### `skifflm openai`

Talk to any local OpenAI-compatible server (including SkiffLLM's own
`--serve`) from the same binary:

```bash
skifflm --serve --host 127.0.0.1 --port 8080 --model model.gguf
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --temp 0.3 --max-tokens 128
```

`--json` (default) prints the raw OpenAI JSON, `--no-json` extracts only the
assistant content, and `--stream` pipes SSE chunks through curl live.

## Interactive Commands

```text
/help                 Show the command list
/info                 Show model and session information
/warmup               Run a short warm-up pass
/history              Show the current conversation
/stats                Show local usage metrics
/compact              Compress the conversation when it is long
/regenerate           Re-run the last user message (/retry)
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
# require a bearer token on the /v1/* endpoints:
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080 --api-key "local-token"
```

Endpoints:

```text
GET  /health                 Simple health check (public)
GET  /v1/models              List the single loaded model (Bearer when --api-key is set)
POST /v1/chat/completions    Chat completion, with OpenAI-style streaming support (Bearer when --api-key is set)
```

Example request:

```bash
# add `--header "Authorization: Bearer local-token"` when --api-key is set
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
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key local-token "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --stream "Tell me a short joke."
python3 scripts/api_client.py http://127.0.0.1:8080 --prompt "Hello" --temperature 0.7
```

The server binds to `127.0.0.1` by default. It responds to CORS preflight
(`OPTIONS`) and sends CORS headers on normal and streaming responses, so
browser-based clients can use it locally. It handles concurrent requests
without blocking health checks and model listing. `/v1/*` endpoints are
auth-less only when you do not pass `--api-key`; with a token set, requests
without a matching `Authorization: Bearer <key>` header receive `401`. If you
bind to `0.0.0.0`, always set `--api-key`.

You can check a running server without loading a model:

```bash
skifflm server health
skifflm server health --json --host 127.0.0.1 --port 8080
```

`server health` reads the same `--host`/`--port`/config values as `--serve`,
so it is useful in scripts and CI.

## Usage Metrics

Every real generation records a plain-text, tab-separated line in
`metrics.txt` next to the history file (default
`~/.local/share/skifflm/metrics.txt`). The file contains only timing and
token counts; no prompts or generated text are written.

```text
timestamp    prompt_tokens    generated_tokens    prompt_ms    generation_ms    tps
```

Inside the interactive shell run:

```text
/stats
```

to print totals (messages, prompt/generated tokens, total time, average
tokens per second) and the metrics file path. Users can inspect or truncate
`metrics.txt` freely; it is never uploaded.

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

Manage the config from the CLI:

```bash
skifflm config path            # print the config file path
skifflm config show            # print the effective configuration
skifflm config init            # write the current effective config to disk
```

`config init` creates the parent directory if needed and writes every loaded
option as `key=value` lines, including repeated `stop=` entries.

## Environment Variables

```text
SKIFFLLM_MODEL       Default model path
SKIFFLLM_MODEL_DIR   Default model directory
SKIFFLLM_CONFIG      Config file path
SKIFFLLM_HISTORY     History file path
SKIFFLLM_SYSTEM      Default system prompt
SKIFFLLM_PROFILE     Default sampling profile
SKIFFLLM_API_KEY     Bearer token for `--serve` (optional)
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

SkiffLLM exposes a single `SKIFFLLM_LLAMA_BACKEND` option that selects the
hardware-acceleration backend at build time:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
cmake --build build -j
./build/skifflm --backend-info   # shows the backends linked into this build
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Supported values: `auto` (default, CPU or platform default), `cuda`, `metal`,
`vulkan`, `opencl`, `blas`, `cpu`. The equivalent CMake presets are
`release-cuda`, `release-vulkan`, `release-metal`, `release-opencl`, and
`release-blas`. GPU/NPU code is never enabled implicitly; you must pass the
backend at configure time and offload layers at runtime with `--gpu-layers`.

For Metal on macOS, the `metal` preset enables the Metal backend.

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

- No network access used by the desktop runtime.
- No remote inference endpoints.
- No cloud API keys or accounts.
- No telemetry, analytics, or crash reporting.
- Prompts and outputs stay on the local machine.

The only optional network use is during the build when CMake downloads the
pinned llama.cpp source tree, plus the explicit `scripts/model_fetch.py`
helper when you ask it to fetch a model. Supply an existing checkout with
`SKIFFLLM_LLAMA_SOURCE_DIR` (or skip the helper) to avoid network use entirely.

## Project Layout

```text
CMakeLists.txt                Build configuration and CMake presets
include/skifflm/              Public API headers
src/                          CLI, core, and local server implementation
tests/                        Unit and integration tests
configs/                      Example configuration
scripts/                      CI, release, model fetch, API client, completions
android/                      Kotlin/Compose Android app and llama.cpp JNI
ios/                          SwiftUI iOS app and llama.cpp Objective-C++ bridge
docs/                         Setup, usage, architecture, FAQ, release docs
.github/                      Issue templates, PR template, CI workflow
CONTRIBUTING.md               Contribution guide
SECURITY.md                   Security policy
CODE_OF_CONDUCT.md            Code of conduct
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

SkiffLLM does not bundle models, and retrieval-augmented generation is not
implemented yet. The local server is public on `127.0.0.1` by default; use
`--api-key` when binding to a non-loopback interface. See
[Known Limitations](docs/LIMITATIONS.md) for an honest list.

## Contributing

Contributions are welcome. Please:

1. Open an issue for major features before submitting a pull request.
2. Keep changes small and reviewable.
3. Add tests for new behavior.
4. Keep the interface English and free of comments in the public source.

## License

SkiffLLM is released under the [MIT License](LICENSE).
