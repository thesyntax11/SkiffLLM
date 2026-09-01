# Usage Guide

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
