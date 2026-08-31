# Benchmarks

SkiffLLM reports prompt tokens, generated tokens, total generation time, and
tokens per second after every run.

## Measure on your machine

Use a fixed random seed and a fixed model:

```bash
skifflm --model model.gguf --prompt "Say hello in three languages." --seed 42
```

JSON mode makes parsing measurements easy:

```bash
skifflm --model model.gguf --prompt "Say hello in three languages." --seed 42 --json
```

## Factors affecting speed

- Model size and quantization (`Q4_K_M`, `Q5_K_M`, `Q8_0`, etc.)
- CPU cores and memory bandwidth
- GPU offload (CUDA, Metal, ROCm, SYCL, Vulkan)
- Context size and batch size
- n-predict
- Concurrent load on the machine
- Compiler flags and CPU backend selection

## Small model configuration reference

These are typical settings for a machine with a modern consumer CPU.

| Use case | Model | Quant | Context | Batch | GPU layers |
| --- | --- | --- | --- | --- | --- |
| Demo / low RAM | SmolLM2-135M-Instruct | Q4_K_M | 1024 | 256 | 0 |
| Daily assistant | Qwen2.5-0.5B-Instruct | Q4_K_M | 2048 | 512 | 0 |
| Quality focus | Qwen2.5-1.5B-Instruct | Q4_K_M | 4096 | 512 | 0 |
| GPU acceleration | Llama-3.2-3B-Instruct | Q4_K_M | 4096 | 512 | -1 |

## Keeping numbers honest

- Use the same prompt.
- Use a deterministic seed.
- Run the same build.
- Close other heavy processes.
- Repeat at least three times and take the median.
