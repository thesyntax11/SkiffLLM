## SkiffLLM

<p align="center">
  <img src="docs/logo.svg" alt="SkiffLLM logo" width="128" height="128"/>
</p>

<p align="center">
  <strong>Run any GGUF model as a Unix pipe.</strong><br/>
  Local. Offline. No cloud accounts, no telemetry, no daemon.
</p>

<p align="center">
  <strong>Languages:</strong>
  <a href="README.md">English</a> ·
  <a href="README.tr.md">Türkçe</a> ·
  <a href="README.de.md">Deutsch</a> ·
  <a href="README.es.md">Español</a> ·
  <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/github/v/release/thesyntax11/SkiffLLM" alt="Latest release"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Platforms"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Offline"/>
  <img src="https://img.shields.io/github/downloads/thesyntax11/SkiffLLM/total" alt="Downloads"/>
  <img src="https://img.shields.io/github/stars/thesyntax11/SkiffLLM" alt="Stars"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/release.yml/badge.svg" alt="Release"/>
</p>

SkiffLLM is a single local AI runtime that feels like a Unix tool. It runs any
GGUF model through llama.cpp on your CPU or GPU, keeps every token on your
machine, and drops straight into your shell workflow.

```bash
git diff | skiffllm "review these changes"
cat error.log | skiffllm "find the root cause"
cat README.md | skiffllm "summarize this"
skiffllm --project . "where is this implemented?"
```

---

## Get started

### One command (published release)

```bash
curl -fsSL https://raw.githubusercontent.com/thesyntax11/SkiffLLM/main/scripts/install-latest.sh | bash
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

### One command (source checkout)

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

### Or build manually

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

### Or install a published release

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

The helper maps your OS/arch to the release archive. It fails fast when a
release has no assets yet.

### Direct download

Every published release carries native artifacts with a stable naming scheme:

| Asset | Use |
| --- | --- |
| `skiffllm-<version>-linux-x86_64.tar.gz` | Linux |
| `skiffllm-<version>-macos-arm64.tar.gz` | macOS Apple Silicon |
| `skiffllm-<version>-windows-x86_64.zip` | Windows archive |
| `skiffllm-<version>-windows-x86_64.exe` | Standalone Windows executable |
| `skiffllm-<version>-linux-x86_64` | Standalone Linux executable |
| `skiffllm-<version>-macos-arm64` | Standalone macOS executable |
| `skiffllm-<version>-Android.apk` | Android device or emulator |
| `skiffllm-<version>-iOS.ipa` | iOS app container |
| `checksums.txt` | SHA-256 for every asset above |
| `<asset>.sha256` | Per-asset SHA-256 integrity file |

On Windows, `skiffllm.exe` opens the desktop GUI. It starts maximized and
offers a model picker, live generation settings, streaming chat output, and a
Stop button. The command-line build is also shipped as `skiffllm-cli.exe` so
the same release can be scripted from PowerShell or a terminal.

On Linux and macOS, mark a standalone executable with `chmod +x` after
downloading it. The iOS `.ipa` is produced as an unsigned app container unless
the repository
has Apple signing secrets configured. A device build normally needs Xcode and
a development team; local iOS setup is documented in [ios/README.md](ios/README.md).

The release workflow can also be run manually from the repository's Actions
page. That produces the same desktop executables, Windows `.exe`, Android APK,
and iOS `.ipa` artifacts without creating a GitHub Release.

### Record a demo

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /path/to/model.gguf
```

This records a real terminal session for the README; it never invents output.

### Get a small model

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Files are saved to `~/.local/share/skiffllm/models` by default. You can also
point `--model` at any existing `.gguf` file.

Model files are downloaded under their own upstream license; SkiffLLM does not
redistribute them. The bundled catalog licenses are:

| Model | License |
| --- | --- |
| Qwen2.5 0.5B / Qwen3 0.6B | Apache-2.0 |
| SmolLM2 1.7B | Apache-2.0 |
| Phi-3.5 Mini | MIT |
| Llama 3.2 1B | Llama 3.2 Community License (attribution + naming terms) |

The Llama 3.2 Community License requires prominent "Built with Llama"
attribution and that redistributed Llama-derived model names begin with
"Llama". Read the upstream license before reuse. The helper records a SHA-256
sidecar as the authority for what you downloaded.

Full setup: [docs/INSTALL.md](docs/INSTALL.md).

---

## Why SkiffLLM

| | SkiffLLM | Cloud assistants | Ollama |
| --- | --- | --- | --- |
| Cloud required | ❌ | ✅ | ❌ |
| Cloud API key | ❌ | ✅ | ❌ |
| Account / signup | ❌ | ✅ | ❌ |
| Telemetry | ❌ | varies | ❌ |
| Runs a daemon | ❌ | ✅ | ✅ |
| Direct GGUF file | ✅ | ❌ | partial |
| CPU-only machines | ✅ | ❌ | ✅ |
| GPU offload | ✅ | n/a | ✅ |
| Unix pipelines | ✅ | ❌ | ❌ |
| Project/code context | ✅ | ❌ | ❌ |
| Local OpenAI-compatible API | ✅ | n/a | ✅ |
| Native Android client | ✅ | ❌ | community |
| Native iOS client | ✅ | ❌ | community |

The runtime never downloads models, never phones home, and never needs an
account. You bring the GGUF file; SkiffLLM brings the inference.

### Why not just Ollama?

Short answer: use Ollama when you want a fast, catalogue-first model server with
one-line downloads; use SkiffLLM when you want the model to behave like a native
Unix tool, an air-gapped component, or an embedded engine inside a CI, desktop,
or mobile workflow — no daemon, one binary, and core inference that never
connects. The honest decision matrix, the real trade-offs, and a task-for-task
migration guide are in [docs/COMPARISON.md](docs/COMPARISON.md).

---

## Highlights

| Capability | Description |
| --- | --- |
| Unix pipelines | `cat file \| skiffllm "summarize"`, `git diff \| skiffllm "review"` |
| Project context | `--project <dir>` adds a real file index + bounded source slice |
| Model manager | `skiffllm model list / info / install / remove / verify` |
| Git integration | `skiffllm git review / explain / commit / log / status` |
| Interactive shell | Streaming token output, history, live token counters |
| File context | `--attach`, `/file`, and `@file` expansion in any prompt |
| Conversation export | `--export` and `/export` save sessions as Markdown |
| Session & memory | Named sessions, `/remember`, `/forget`, persistent facts |
| Local API server | `--serve` exposes an OpenAI-compatible endpoint |
| Server protection | Optional `--api-key` Bearer auth on `/v1/*` |
| Real benchmark | `--benchmark <runs>` measures actual prompt and generation speed |
| Sampling profiles | `balanced`, `fast`, `creative`, `code`, `precise` |
| Advanced sampling | temperature, top-p, top-k, min-p, typical-p, penalties |
| Context management | automatic trimming, reserved generation space, `/compact` |
| JSON mode | machine-readable output for scripts and tooling |
| Diagnostics | `--doctor` system report, `--model-info`, `--tokenize` |
| Native mobile apps | Android (Jetpack Compose) and iOS (SwiftUI) |

---

## Practical examples

```bash
# Review a change set before you push
git diff | skiffllm "review these changes"

# Find the real cause in a messy log
journalctl -e | skiffllm "find suspicious errors and a likely root cause"

# Summarize a file you just read
cat README.md | skiffllm "summarize this"

# Point it at an entire repository
skiffllm --project . "where is authentication implemented?"

# Machine-readable output for your own scripts
git diff | skiffllm --json "classify this diff"

# Safe code review (proposes a diff, never edits files)
skiffllm --code --project . "fix the bug in src/server.cpp"
```

## Model manager

SkiffLLM stays offline at runtime. Model retrieval is an explicit, separate
command.

```bash
skiffllm model list
skiffllm model info qwen2.5-0.5b
skiffllm model install qwen2.5-0.5b
skiffllm model verify qwen2.5-0.5b --update
skiffllm model remove qwen2.5-0.5b --force
```

`model install` delegates to `scripts/model_fetch.py`, which downloads exactly
one GGUF over HTTPS from Hugging Face, checks the GGUF header, and records a
SHA-256 sidecar in your model directory. The catalog size is advisory because a
model maintainer can re-upload a revision with a different size; the sidecar is
the authoritative integrity check. Inference itself never opens a connection.

## Git integration

Local, offline code review and explanation for the diff in front of you.

```bash
# The git subcommands read the diff themselves; no pipe is needed.
skiffllm git review
skiffllm git review --cached
skiffllm git explain
skiffllm git commit --cached
skiffllm git log
skiffllm git status
```

`git commit --cached` proposes a conventional commit message from your staged
diff; it does not run `git commit` for you.

## Sessions & persistent memory

```bash
skiffllm --session coding --model qwen2.5-0.5b-instruct-q4_k_m.gguf
skiffllm --session writing --model qwen2.5-0.5b-instruct-q4_k_m.gguf

skiffllm session list
skiffllm session show coding
skiffllm session rename coding writing
skiffllm session remove old-draft
```

Persistent memory lives in `~/.local/share/skiffllm/memories.txt` and never
leaves the machine.

```bash
skiffllm --remember "the user prefers concise answers"
skiffllm --forget concise
```

Inside the interactive shell use `/remember`, `/forget`, `/memories`,
`/clear-memories`, `/compact`, `/regenerate`, and `/export`.

---

## Local OpenAI-compatible server

```bash
# local only
skiffllm --model model.gguf --serve --host 127.0.0.1 --port 8080

# non-loopback listener protected by a shared token
skiffllm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Endpoints:

```text
GET  /health                 public health check
GET  /version                public version check
GET  /v1/models              Bearer protected when --api-key is set
POST /v1/chat/completions    Bearer protected when --api-key is set
```

The server supports OpenAI-style streaming (`"stream": true`) and answers fast
endpoints while a generation runs. Chat generation is serialized because a
llama.cpp context is not thread-safe.

```python
from openai import OpenAI

client = OpenAI(base_url="http://127.0.0.1:8080/v1", api_key="local-token")
resp = client.chat.completions.create(
    model="qwen2.5-0.5b-instruct-q4_k_m",
    messages=[{"role": "user", "content": "Explain this diff."}],
    stream=True,
)
for chunk in resp:
    print(chunk.choices[0].delta.content or "", end="")
```

A dependency-free Python client is included, so you never need `curl` for the
server:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

(The `openai` and `server health` subcommands invoke `curl` internally; install
it on macOS/Linux or use `scripts/api_client.py` if you prefer no external
dependency.)

---

## Mobile apps

SkiffLLM ships native clients for [Android](android/README.md) and
[iOS](ios/README.md). Both run supported GGUF models entirely on device,
stream tokens, show a live context-usage bar, and expose the same feature
surface as the desktop CLI:

- per-conversation sampling settings and code mode
- multi-conversation create/open/rename/delete/backup/import
- persistent facts, quick prompts, and share-sheet intake
- stop sequences, sampling profiles, and conversation compaction
- model warm-up, real 3-round benchmark, session statistics
- text/JSON/XML file attachment (read locally, capped)
- Markdown export and one-tap copy
- GGUF import with header verification

Inference itself never opens a connection. There are exactly two network paths,
both explicit and user-initiated: `model install` downloads from Hugging Face,
and the `openai` subcommand talks to an HTTP server you point it at. The
downloaded file must have a valid GGUF header and gets a SHA-256 sidecar; the
catalog size is advisory because an upstream revision can change. Android
device backups are disabled so prompts and history never leave the device.

---

## Command line

```text
Usage: skiffllm [options] [model.gguf]

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

Subcommands:
  run [prompt] [opts]        One-shot prompt
  model list|info|install|remove|verify
  chat-template list|detect|info
  openai [prompt] [opts]     Send a prompt to a local OpenAI-compatible server
  config path|show|init      Manage the config file (show --json emits JSON)
  server health [--json]     Check a running local server
```

Full usage: [docs/usage.md](docs/usage.md). Interactive commands:
`/help`, `/warmup`, `/history`, `/stats`, `/compact`, `/regenerate`,
`/tokenize`, `/file`, `/clear-attach`, `/clear`, `/reset`, `/system`,
`/model`, `/profile`, `/stop`, `/temp`, `/top-p`, `/top-k`, `/min-p`,
`/typical`, `/n`, `/ctx`, `/export`, `/save`, `/exit`.

---

## Benchmark honesty

```bash
skiffllm --model model.gguf --benchmark 3
skiffllm --model model.gguf --benchmark 3 --json
```

Every number is measured on your machine with your model and hardware.
SkiffLLM never invents benchmark results. See
[docs/benchmarks.md](docs/benchmarks.md) for the methodology and an empty
results table waiting for real contributed runs.

## Privacy proof

```bash
skiffllm --doctor --network
```

prints the runtime facts: core generation makes no outbound calls, there is no
telemetry or cloud API, and history is stored locally. The only network paths
are explicit and user-initiated: `model install` (Hugging Face download) and
the `openai` subcommand (a server you point it at). `--serve` only opens a
local listener and never dials out.

---

## Enterprise & operations

SkiffLLM is built to be deployed, hardened, and audited as an inference
component rather than as a chat daemon.

- [docs/ENTERPRISE.md](docs/ENTERPRISE.md) — production deployment, server
  hardening, model supply chain, air-gapped and CI reference topologies,
  sizing, and the honest runbook.
- [docs/OLLAMA_MIGRATION.md](docs/OLLAMA_MIGRATION.md) — task-for-task mapping
  from Ollama habits to SkiffLLM.
- [configs/enterprise.example.conf](configs/enterprise.example.conf) — locked
  down config (loopback server, pinned model, deterministic sampling).
- Docker — multi-stage server image and loopback compose stack in
  `docker/`; models are mounted read-only, never baked into the image.
- `scripts/enterprise-check.sh` — non-destructive preflight (binary, model,
  SHA-256 sidecar, config, disk, backends).
- `scripts/check-links.sh` — validates every relative Markdown link in the
  README and docs before a release.

---

## Install

```bash
bash scripts/install.sh --help
bash scripts/install.sh --prefix "$HOME/.local"
bash scripts/install.sh --prefix /usr/local --backend metal
```

Convenience `Makefile`:

```bash
make release
make tests
make check
make install
make help
```

Prebuilt archives follow `skiffllm-<version>-<os>-<arch>.tar.gz` (for example
`skiffllm-v1.6.0-linux-x86_64.tar.gz`, `.zip` on Windows) with a `checksums.txt`
when published. See [docs/INSTALL.md](docs/INSTALL.md).
Shell completions are in [scripts/completions](scripts/completions/).

## Build options

| Option | Default | Description |
| --- | --- | --- |
| `SKIFFLLM_BUILD_TESTS` | `ON` | Build and register the test suite |
| `SKIFFLLM_FETCH_LLAMA` | `ON` | Download and build a pinned llama.cpp |
| `SKIFFLLM_LLAMA_SOURCE_DIR` | empty | Use an existing llama.cpp checkout |
| `SKIFFLLM_BUILD_SHARED_LLAMA` | `OFF` | Build llama.cpp as a shared library |
| `SKIFFLLM_USE_READLINE` | `ON` | Enable GNU Readline when available |
| `SKIFFLLM_LLAMA_BACKEND` | `auto` | `cuda`, `metal`, `vulkan`, `opencl`, `blas`, `cpu` |

Hardware acceleration is always explicit: select the backend at configure time
and offload layers at runtime with `--gpu-layers`.

---

## Requirements

- A C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- CMake 3.20+
- Optional CMake `FetchContent` for a pinned llama.cpp
- A GGUF model file
- Optional: CUDA/Metal/Vulkan/OpenCL/BLAS for hardware acceleration

---

## Project layout

```text
include/skiffllm/              Public API headers
src/                          CLI, core, and local server implementation
tests/                        Unit tests (model-free)
configs/                      Example configuration
scripts/                      CI, release, model fetch, API client, completions
android/                      Kotlin/Compose Android app and llama.cpp JNI
ios/                          SwiftUI iOS app and llama.cpp Objective-C++ bridge
docs/                         Setup, usage, architecture, FAQ, release docs
.github/                      Issue templates, PR template, CI workflow
```

---

## Roadmap

- Embedding and RAG mode
- Package manager integration (Homebrew, apt, vcpkg)
- GPU benchmark matrix across backends
- Grammar-constrained generation
- Token-level sampling diagnostics
- Multi-worker generation across multiple models

## Known limitations

SkiffLLM does not bundle models. The local server is public on `127.0.0.1` by
default; use `--api-key` when binding to a non-loopback interface. See
[docs/LIMITATIONS.md](docs/LIMITATIONS.md).

## Contributing

Contributions are welcome. Keep changes small, keep the offline promise, never
invent benchmark numbers, and add tests for new behavior. See
[CONTRIBUTING.md](CONTRIBUTING.md) and
[docs/GOOD_FIRST_ISSUES.md](docs/GOOD_FIRST_ISSUES.md).

## License

MIT — see [LICENSE](LICENSE).
