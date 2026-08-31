# Setup

SkiffLLM has no runtime network dependencies. You build it once and run it
against a local GGUF model.

## Requirements

- A C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- CMake 3.20 or newer
- Optional: GNU Readline for line editing and shell history
- Optional: a CUDA, Metal, ROCm, SYCL, or Vulkan backend for GPU offload
- A GGUF model file

## Build

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Use an existing llama.cpp checkout to avoid the optional configure-time
download:

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp
```

## Install

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Get a model

SkiffLLM never downloads a model. Copy a GGUF file you already have into the
model directory or pass its path directly:

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

Suggested small models for a fast CPU-only start:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

## First run

```bash
./build/skifflm --doctor
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf --model-info
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf
```

## Run the tests

```bash
ctest --test-dir build --output-on-failure
```

Or run all local checks:

```bash
scripts/ci-local.sh
```

## Configuration

The default config file is `~/.config/skifflm/config`. A complete example is
at `configs/skifflm.example.conf`. CLI flags override config file values.

## Troubleshooting

- If no model is found, run `--list-models` or pass `--model`.
- If generation stops at the context limit, raise `--ctx` or shorten the
  conversation.
- If a prebuilt llama.cpp is available, set `SKIFFLLM_LLAMA_SOURCE_DIR`.
- On multiple-socket systems, try `--numa`.
- For faster CPU inference, keep `--profile fast` and a quantized model.
