# Usage Guide

## Unix pipeline mode

SkiffLLM auto-detects a piped stdin, so the model becomes part of a shell
workflow:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

With no instruction argument, the piped text itself is the prompt. With an
instruction argument, the piped text becomes `<context>`.

## Project context

```bash
skifflm --project . "where is authentication handled?"
```

`--project <dir>` builds a bounded file index plus a slice of source/config
content before generation. It skips `.git`, build dirs, caches, and vendored
dependencies.

## Sessions & memory

```bash
skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft

skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Interactive shell commands: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (compress a long conversation
into a bullet summary while preserving facts), and `/regenerate` or `/retry`
(re-run the last user message with the current sampling settings).

## Summarize shortcut

```bash
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Model manager

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model verify` checks the GGUF magic header, the expected file size, and a
SHA-256 sidecar whenever one exists. `--update` records a local SHA-256
sidecar. Downloads from `model_fetch.py` also write that sidecar automatically
and support `--verify` without re-downloading.

Inference stays offline. `model install` runs the explicit `model_fetch.py`
helper over HTTPS.

## One-shot run and chat templates

```bash
skifflm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skifflm chat-template list
skifflm chat-template detect --model model.gguf
```

## OpenAI client

```bash
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Hardware acceleration

```bash
skifflm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Backends are chosen at build time; `--backend-info` reports what is actually
linked.

## Git integration

```bash
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

## Safe code mode

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` produces a unified-diff proposal and never edits a file itself.

## Interactive Mode

```bash
skifflm --model ~/models/model-q4_k_m.gguf
```

The interface is a prompt named `you>`. Type a message and press Enter. Text is
streamed as it is generated.

## One-shot Mode

```bash
skifflm --model model.gguf --prompt "What is recursion?"
```

## Named Sessions

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

Each named session gets its own history file.

## System Prompt

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Profiles

```bash
skifflm --model model.gguf --profile code
```

Available profiles: `balanced`, `fast`, `creative`, `code`, `precise`.

## File Context

```bash
skifflm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skifflm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Repeat `--attach`, use `@path` in the prompt, or manage attachments inside the
shell with `/file` and `/clear-attach`.

## Conversation Export

```bash
skifflm --export conversation.md
```

Exporting the loaded session needs no model and writes Markdown. Inside the
shell use `/export <path>`.

## Chat Template and Warmup

```bash
skifflm --model model.gguf --chat-template chatml
skifflm --model model.gguf --warmup
```

`--chat-template` overrides the model's built-in prompt format name.
`--warmup` runs a one-token generation on startup to reduce first-answer latency.
Inside the shell you can also warm the model again with `/warmup`.

## Interactive Line Editing

When GNU Readline is available, SkiffLLM enables arrow-key navigation, history
lookup, and editable input lines. Without Readline it falls back to plain
`getline`. Streaming output also shows a live token counter on interactive
terminals.

## Stop Sequences

```bash
skifflm --model model.gguf --stop "END" --stop "STOP"
```

Generation stops at the first configured sequence.

## JSON Mode

```bash
skifflm --model model.gguf --prompt "Say hello" --json
```

This disables the interactive shell and writes a single JSON object to stdout.

## Piping

```bash
cat prompt.txt | skifflm --model model.gguf
printf 'Explain this command.' | skifflm --model model.gguf --prompt-file /dev/stdin
```

## Output Files

```bash
skifflm --model model.gguf --prompt-file input.txt --output output.md
```

## Config File

```bash
skifflm --config ~/.config/skifflm/config
```

The default location is used automatically when present.

## Diagnostics

```bash
skifflm --doctor
skifflm --model model.gguf --model-info
skifflm --model model.gguf --smoke
skifflm --model model.gguf --tokenize "hello world"
```

## Local API Server

```bash
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080
```

Endpoints:

```text
GET  /health
GET  /v1/models
POST /v1/chat/completions
```

The server is offline, binds to localhost by default, and handles one request
at a time. It supports OpenAI-style streaming with `"stream": true`.

A quick client is included:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
```

## Benchmark

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Runs a real generation and reports measured prompt time, generation time, and
tokens per second. `--n-predict` controls the run length up to 128 tokens per
run.

## GPU Offload

On a CUDA machine:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skifflm --model model.gguf --gpu-layers -1
```

On macOS, the Metal backend is available by default.

## Model Listing

```bash
skifflm --model-dir ~/models --list-models
```
