# Architecture

SkiffLLM is split into a small core library and a thin CLI layer.

## Components

### `config`

Configuration, argument parsing, environment variables, model discovery, and
sampling profiles.

### `engine`

The llama.cpp wrapper. It owns the model, context, vocabulary, sampler chain,
and generation loop.

### `session`

Persistent conversation storage. Messages and the system prompt are written to
a compact binary file.

### `terminal`

ANSI color helpers, banner output, interactive prompt, and command help.

### `main`

Program entry point, signal handling, config precedence, JSON mode, output
files, one-shot mode, and interactive command dispatch.

## Data Flow

```text
main
  -> config            parse args / env / config file
  -> terminal          setup color and UI
  -> session           load conversation
  -> engine.load       load model and context
  -> engine.generate   tokenize prompt, decode, sample, stream
  -> session.save      optionally persist
```

## Chat Prompting

`engine.build_prompt` applies the model's chat template through
`llama_chat_apply_template`. If the template is unavailable, it falls back to
the ChatML template.

## Context Management

- The context is reset before each call.
- The prompt is sent in batches sized by `--batch`.
- When the prompt exceeds the available context, `engine` drops the oldest
  non-system messages when `--auto-trim` is enabled.
- `--reserve-ctx` reserves part of the window for generated output.

## Sampling

The sampler chain is ordered:

1. Top-K
2. Top-p
3. Min-p
4. Typical-p
5. Repetition penalties
6. Temperature
7. Distribution

All samplers use the public llama.cpp sampling API.

## Extending

- Add configuration under `config.hpp` and `config.cpp`.
- Add engine capabilities under `engine.hpp` and `engine.cpp`.
- Add tests in `tests/test_main.cpp`.
- Update README, CHANGELOG, and help text.
