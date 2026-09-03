# Setup

SkiffLLM's core inference has no runtime network dependencies. You build it
once and run it against a local GGUF model. The only runtime network paths are
the explicit `model install` downloader and the `openai` client (which you
point at a server); `--serve` only opens a local listener.

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

Or use the convenience installer with an explicit GPU/NPU backend:

```bash
bash scripts/install.sh                            # CPU / platform default
BACKEND=cuda bash scripts/install.sh               # CUDA
BACKEND=vulkan bash scripts/install.sh             # Vulkan
BACKEND=metal bash scripts/install.sh              # macOS Metal
./build/skifflm --backend-info                     # inspect linked backends
```

Use an existing llama.cpp checkout to avoid the optional configure-time
download:

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp --backend cuda
```

## Install

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Get a model

The SkiffLLM runtime never downloads a model. You can copy a GGUF file you
already have into the model directory or pass its path directly. If you want a
recommended quantized model, run the optional fetch helper once:

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

The helper stores the file in `~/.local/share/skifflm/models`. Inference stays
fully offline.

Or copy one yourself:

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
