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
