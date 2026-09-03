# Benchmarks

SkiffLLM reports prompt tokens, generated tokens, prompt time, generation time,
and tokens per second after every real run. No number in this document is
invented: every published row below is produced by `--benchmark` on the exact
hardware/model/quant listed in that row.

## How to reproduce a measurement

Use the built-in benchmark so the methodology is identical for every machine:

```bash
skiffllm --model /path/model-Q4_K_M.gguf \
  --benchmark 3 \
  --temp 0.70 --top-p 0.95 --top-k 40 \
  --ctx 2048 --batch 512 --seed 42
```

The benchmark uses a fixed prompt, a fixed seed, up to 128 tokens per run
(change with `--n-predict`), and reports real timing from the engine.

For independent verification, run the same command three times and record the
median. Also record the exact llama.cpp version and SkiffLLM version:

```bash
./build/skiffllm --version
./build/skiffllm --backend-info
```

## Measurement protocol

- Same prompt and same sampling seed.
- Same build of llama.cpp (the project pins a commit in `CMakeLists.txt`).
- Same context size, batch size, and threads.
- Same quantization and same file (record the SHA-256).
- At least three runs; report the median `prompt_ms`, `generation_ms`, and
  `tokens_per_second`.
- Close other heavy processes while running.
- Record hardware: CPU model, architecture, RAM, GPU model and driver, and
  how many layers were offloaded.

## How to publish a result

A contribution is accepted when it includes all columns from the table below
and a `skiffllm --version` / `--backend-info` output plus the model SHA-256.
Keep numbers separated by hardware so readers can compare like-for-like.

## Results

No results are published yet. The maintainers intentionally have not filled in
this table with marketing numbers; a real row requires a maintainer to run the
command above on the listed machine and attach the output.

| Model | Quant | Hardware | Threads/backend | Context | Prompt ms | Gen tok/s | Median run |
| --- | --- | --- | --- | --- | --- | --- | --- |
| — | — | — | — | — | — | — | — |

> The previous draft of this document contained an example row with example
> numbers. No such values are real measurements and they have been removed.
> Treat every number in this table as a real contribution only.

## How to contribute a result

Create a pull request that:

1. Adds one row with the exact columns above.
2. Includes the `skiffllm --version` and `--backend-info` output in the PR
   description.
3. Includes the model file's SHA-256 and the exact command used.
4. States where the measurement was run (desktop, phone, server).

Reviewers will reject rows without the attached command output.

## Factors affecting speed

- Model size and quantization (`Q4_K_M`, `Q5_K_M`, `Q8_0`, etc.)
- CPU cores and memory bandwidth
- GPU offload (CUDA, Metal, ROCm, SYCL, Vulkan, OpenCL)
- Context size and batch size
- `n-predict`
- Concurrent load on the machine
- Compiler flags and CPU backend selection
- OS-level power modes (especially battery vs. plugged in on mobile)

## Small model configuration reference

These are starting points for a general-purpose local assistant. They are not
performance claims.

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
- Never extrapolate a phone or laptop number to "all machines".
