# Model recommendations for Android

These are the models to use for a smooth on-device chat experience. Every
entry is a GGUF build from the upstream `ggml-org` or official `lmstudio-community`
repositories; use the `Q4_K_M` quant unless a device has spare RAM.

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
- **High-end phone with 8 GB RAM or more**: Phi-3.5-mini-instruct Q4_K_M. It has
  noticeably better reasoning, but it is slower and uses more memory.
- **When using the app without GPU layers**: use 1 GB or less model to avoid
  memory pressure.

## Where to get them

All projects publish GGUF files directly on Hugging Face. There is no built-in
download client, so store the file on the phone first and load it from the
Android file picker.

- `Qwen/Qwen2.5-0.5B-Instruct-GGUF`
- `bartowski/Qwen_Qwen3-0.6B-GGUF`
- `bartowski/Llama-3.2-1B-Instruct-GGUF`
- `bartowski/Phi-3.5-mini-instruct-GGUF`
- `bartowski/SmolLM2-1.7B-Instruct-GGUF`

Pick the `Q4_K_M.gguf` file in each repository. For a phone with 4 GB or less
RAM, choose `Q4_K_M` from the 0.5B or 0.6B models above.

## Notes

- Token speed is dominated by CPU single-thread performance and memory
  bandwidth on phones without GPU support.
- The app reports real measurements, not marketing estimates.
- A smaller context window gives a snappier response and lower memory use.
