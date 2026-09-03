# Usage Guide

## Unix pipeline mode

SkiffLLM auto-detects a piped stdin, so the model becomes part of a shell
workflow:

```bash
git diff | llm "review these changes"
cat error.log | llm "find the root cause"
cat README.md | llm "summarize this"
llm --project . "where is authentication handled?"
```

With no instruction argument, the piped text itself is the prompt. With an
instruction argument, the piped text becomes `<context>`.

## Project context

```bash
llm --project . "where is authentication handled?"
```

`--project <dir>` builds a bounded file index plus a slice of source/config
content before generation. It skips `.git`, build dirs, caches, and vendored
dependencies. It is a one-shot option: pair it with a prompt (for example
`llm --project . "where is auth implemented?"`), not with the interactive
shell.

## Sessions & memory

```bash
llm session list
llm session show coding
llm session rename coding writing
llm session remove old-draft

llm --remember "the user prefers concise answers"
llm --forget concise
```

Interactive shell commands: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (compress a long conversation
into a bullet summary while preserving facts), and `/regenerate` or `/retry`
(re-run the last user message with the current sampling settings).

## Summarize shortcut

```bash
llm --summarize README.md
llm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Model manager

```bash
llm model list
llm model info qwen2.5-0.5b
llm model install qwen2.5-0.5b
llm model verify qwen2.5-0.5b
llm model verify qwen2.5-0.5b --update
llm model remove qwen2.5-0.5b --force
```

`model verify` checks the GGUF magic header and verifies the SHA-256 sidecar
whenever one exists. The catalog size is advisory, so a newer upstream revision
is not rejected purely because its byte count changed. `--update` records a
local SHA-256 sidecar. Downloads from `model_fetch.py` also write that sidecar
automatically and support `--verify` without re-downloading.

Inference stays offline. `model install` runs the explicit `model_fetch.py`
helper over HTTPS, and the `openai` subcommand sends a prompt to a server you
choose (default localhost). `--serve` only opens a local listener.

## One-shot run and chat templates

```bash
llm run "Hello" --ctx 2048 --temp 0.3 --threads 4
llm chat-template list
llm chat-template detect --model model.gguf
```

## OpenAI client

```bash
llm openai "Merhaba" --base-url http://127.0.0.1:8080
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

This is an explicit network client. It sends the prompt to the HTTP server you
point it at (default `http://127.0.0.1:8080`), so keep it on localhost unless
you really intend to talk to a remote endpoint. Unlike core inference, it is
not offline.

## Hardware acceleration

```bash
llm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLM_LLAMA_BACKEND=cuda
./build/llm --model model.gguf --gpu-layers -1 --flash-attn
```

Backends are chosen at build time; `--backend-info` reports what is actually
linked.

## Git integration

```bash
llm git review --cached
llm git explain
llm git commit --cached
llm git log
llm git status
```

## Safe code mode

```bash
llm --code --project . "fix the bug in src/server.cpp"
```

`--code` produces a unified-diff proposal and never edits a file itself. It is
a one-shot option: combine it with a prompt or a pipe (`git diff | llm
--code --project . "propose a fix"`). It does not apply inside the interactive
shell.

## Interactive Mode

```bash
llm --model ~/models/model-q4_k_m.gguf
```

The interface is a prompt named `you>`. Type a message and press Enter. Text is
streamed as it is generated.

## One-shot Mode

```bash
llm --model model.gguf --prompt "What is recursion?"
```

## Named Sessions

```bash
llm --model model.gguf --session writing
llm --model model.gguf --session coding
```

Each named session gets its own history file.

## System Prompt

```bash
llm --model model.gguf --system "You are a patient Python tutor."
```

## Profiles

```bash
llm --model model.gguf --profile code
```

Available profiles: `balanced`, `fast`, `creative`, `code`, `precise`.

## File Context

```bash
llm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
llm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Repeat `--attach`, use `@path` in the prompt, or manage attachments inside the
shell with `/file` and `/clear-attach`.

## Conversation Export

```bash
llm --export conversation.md
```

Exporting the loaded session needs no model and writes Markdown. Inside the
shell use `/export <path>`.

## Chat Template and Warmup

```bash
llm --model model.gguf --chat-template chatml
llm --model model.gguf --warmup
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
llm --model model.gguf --stop "END" --stop "STOP"
```

Generation stops at the first configured sequence.

## JSON Mode

```bash
llm --model model.gguf --prompt "Say hello" --json
```

This disables the interactive shell and writes a single JSON object to stdout.

## Piping

```bash
cat prompt.txt | llm --model model.gguf
printf 'Explain this command.' | llm --model model.gguf --prompt-file /dev/stdin
```

## Output Files

```bash
llm --model model.gguf --prompt-file input.txt --output output.md
```

## Config File

```bash
llm --config ~/.config/llm/config
```

The default location is used automatically when present.

## Diagnostics

```bash
llm --doctor
llm --model model.gguf --model-info
llm --model model.gguf --smoke
llm --model model.gguf --tokenize "hello world"
```

## Local API Server

```bash
# local-only
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# protected non-loopback listener
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$LLM_SERVER_KEY"
```

Endpoints:

```text
GET  /health                 public health check
GET  /v1/models              bearer protected when --api-key is set
POST /v1/chat/completions    bearer protected when --api-key is set
```

The server is offline, binds to localhost by default, and supports
OpenAI-style streaming with `"stream": true`. Fast endpoints answer while a
generation runs; chat generation is serialized behind a mutex. When
`--api-key` is configured, `/v1/*` requires
`Authorization: Bearer <key>` and returns `401` otherwise.

A quick client is included:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$LLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
llm --model model.gguf --benchmark 3
llm --model model.gguf --benchmark 3 --json
```

Runs a real generation and reports measured prompt time, generation time, and
tokens per second. `--n-predict` controls the run length up to 128 tokens per
run.

## GPU Offload

On a CUDA machine:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
llm --model model.gguf --gpu-layers -1
```

On macOS, the Metal backend is available by default.

## Model Listing

```bash
llm --model-dir ~/models --list-models
```
