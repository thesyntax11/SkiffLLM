# Model recommendations for Android

These are the models to use for a smooth on-device chat experience. The list
uses published GGUF builds: `Qwen` publishes its own quantizations, while the
other rows are `bartowski` quantizations of the original models. Use the
`Q4_K_M` quant unless a device has spare RAM.

| Model | Parameters | Q4_K_M file | Typical working set | Minimum hardware |
| --- | --- | --- | --- | --- |
| Qwen2.5-0.5B-Instruct | 0.49B | ~0.5 GB | ~1 GB / 1.5 GB | Any Android phone |
| Qwen3-0.6B-Instruct | 0.6B | ~0.5 GB | ~1 GB / 1.5 GB | Any Android phone |
| Llama-3.2-1B-Instruct | 1.24B | ~0.9 GB | ~1.5 GB / 2.5 GB | Mid-range phone |
| Phi-3.5-mini-instruct | 3.8B | ~2.4 GB | ~4 GB / 6 GB | 8 GB+ RAM phone |
| SmolLM2-1.7B-Instruct | 1.7B | ~1.1 GB | ~2 GB / 3 GB | Mid-range phone |

File sizes are the published Q4_K_M GGUF sizes. Working set is a rough estimate
for the model plus the context window; it varies by phone and context size.

## Choosing the right one

- **Old/low-memory phone**: Qwen2.5-0.5B-Instruct Q4_K_M. It is fast, small,
  and still understands instructions and follows chat format.
- **Typical modern phone**: Qwen3-0.6B-Instruct or Llama-3.2-1B-Instruct Q4_K_M.
  These are the best balance of quality and speed.
  Qwen3 models are instruction models with an optional thinking mode. If the
  answer starts with long reasoning text, put `/no_think` at the start of the
  system prompt to skip it on mobile.
- **High-end phone with 8 GB RAM or more**: Phi-3.5-mini-instruct Q4_K_M. It has
  noticeably better reasoning, but it is slower and uses more memory.
- **When using the app without GPU layers**: use 1 GB or less model to avoid
  memory pressure.

## Get them

The Android app can download these models directly from Hugging Face:
`Settings` → `Models` → `Download`. It uses HTTPS, shows progress, can be
cancelled, and checks the GGUF header before loading.

The exact files used by the in-app catalog are:

- `Qwen/Qwen2.5-0.5B-Instruct-GGUF` → `qwen2.5-0.5b-instruct-q4_k_m.gguf`
- `bartowski/Qwen_Qwen3-0.6B-GGUF` → `Qwen_Qwen3-0.6B-Q4_K_M.gguf`
- `bartowski/Llama-3.2-1B-Instruct-GGUF` → `Llama-3.2-1B-Instruct-Q4_K_M.gguf`
- `bartowski/Phi-3.5-mini-instruct-GGUF` → `Phi-3.5-mini-instruct-Q4_K_M.gguf`
- `bartowski/SmolLM2-1.7B-Instruct-GGUF` → `SmolLM2-1.7B-Instruct-Q4_K_M.gguf`

For a phone with 4 GB or less RAM, choose `Q4_K_M` from the 0.5B or 0.6B models
above. You can also download a GGUF manually and use `Browse device` to load it.

## Notes

- Token speed is dominated by CPU single-thread performance and memory
  bandwidth on phones without GPU support.
- The app reports real measurements, not marketing estimates.
- A smaller context window gives a snappier response and lower memory use.
