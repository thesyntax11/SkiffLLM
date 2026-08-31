# SkiffLLM

SkiffLLM is an offline-first local LLM terminal assistant written in modern C++ and powered by llama.cpp.

It runs entirely on your machine. There are no network calls, no cloud APIs, no telemetry, no accounts, and no recurring costs. Once you have a GGUF model file on disk, SkiffLLM works forever without internet access.

## Features

- Fully offline inference engine
- No API keys, accounts, telemetry, or paid services
- Interactive terminal chat with persistent sessions
- Single prompt mode for scripting and pipelines
- Automatic GGUF discovery in the model directory
- Optional GPU offload through llama.cpp
- Streaming token output
- System prompt support
- Configurable sampling parameters
- Session history persisted to disk
- Slash commands inside the interactive shell
- Unit tests and CMake integration
- English interface and documentation

## Requirements

- A C++17 compiler
  - GCC 10 or newer
  - Clang 12 or newer
  - MSVC 2019 or newer
- CMake 3.20 or newer
- A Git checkout of llama.cpp, either fetched automatically or supplied through `SKIFFLLM_LLAMA_SOURCE_DIR`
- A GGUF model file
- Optional: a CPU with many cores, or a CUDA/Metal GPU for faster generation

SkiffLLM itself has no third-party runtime dependency beyond llama.cpp.

## Build

Clone the repository and configure the build:

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The default configuration fetches a pinned revision of llama.cpp and builds it automatically.

To build against an existing llama.cpp source tree:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp
```

To disable the automatic download and still build using a local checkout, set the source directory and disable fetching:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DSKIFFLLM_FETCH_LLAMA=OFF
```

To build a shared llama.cpp library:

```bash
cmake -S . -B build -DSKIFFLLM_BUILD_SHARED_LLAMA=ON
```

To disable tests:

```bash
cmake -S . -B build -DSKIFFLLM_BUILD_TESTS=OFF
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

## Installation

```bash
cmake --install build --prefix /usr/local
```

The binary is named `skifflm`.

## Getting a Model

SkiffLLM works with standard GGUF files. Place a GGUF model in the default model directory:

```text
~/.local/share/skifflm/models/
```

or pass the model path directly.

Good starting points for small, fast, fully offline models:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

Any instruct-tuned model with a chat template works well.

## Quick Start

Run the interactive shell:

```bash
skifflm --model ~/models/qwen2.5-0.5b-instruct-q4_k_m.gguf
```

Run a single prompt:

```bash
skifflm --model model.gguf --prompt "Write a short poem about the sea."
```

Read a prompt from a file:

```bash
skifflm --model model.gguf --prompt-file prompt.txt
```

Pipe a prompt through stdin:

```bash
echo "Summarize this command in one sentence" | skifflm --model model.gguf
```

Use a system prompt:

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Command Line Options

```text
Model options:
  --model <path>          Path to a GGUF model file
  --model-dir <path>      Directory scanned for a GGUF model
  --ctx <n>               Context size (default: 4096)
  --batch <n>             Batch size (default: 512)
  --threads <n>           CPU threads (0 means auto)
  --gpu-layers <n>        GPU layers to offload (-1 means all)
  --mmap                  Enable mmap (default)
  --no-mmap               Disable mmap
  --mlock                 Lock model memory

Generation options:
  --temp <t>              Sampling temperature (default: 0.70)
  --top-p <p>             Nucleus sampling (default: 0.95)
  --top-k <n>             Top-K sampling (default: 40)
  --repeat-penalty <p>    Repeat penalty (default: 1.10)
  --repeat-last-n <n>     Repeat penalty window (default: 64)
  --n-predict <n>         Max generated tokens (default: 512)
  --seed <n|random>       Sampling seed (default: random)

Session options:
  --system <text>         System prompt
  --history <path>        Session history file
  --reset-history         Ignore and overwrite saved history
  --no-save               Do not persist the session

Program options:
  --prompt <text>         Single prompt mode
  --prompt-file <path>    Read the single prompt from a file
  --stdin                 Read the single prompt from stdin
  --non-interactive       Disable the interactive shell
  --color                 Force ANSI colors (default: auto)
  --no-color              Disable ANSI colors
  --verbose               Print llama.cpp logs
  --config <path>         Config file path
  --show-config           Print the effective configuration
  --help                  Show help
  --version               Show version
```

## Interactive Commands

```text
/help                 Show the command list
/info                 Show model and session information
/history              Show the current conversation
/clear                Clear the conversation history
/reset                Clear history and restore the default system prompt
/system <text>        Set or show the system prompt
/model <path>         Reload the model from a GGUF file
/temp <value>         Change sampling temperature
/top-p <value>        Change nucleus sampling threshold
/top-k <value>        Change top-k sampling value
/n <value>            Change maximum generated tokens
/save                 Save the session history now
/exit or /quit        Exit SkiffLLM
```

Any line that is not a command is sent to the model.

## Configuration File

SkiffLLM reads a simple key-value config file. The default location is:

```text
~/.config/skifflm/config
```

Example:

```text
model=/home/user/models/qwen2.5-0.5b-instruct-q4_k_m.gguf
model-dir=/home/user/models
ctx=4096
threads=8
gpu-layers=-1
temp=0.65
top-p=0.92
top-k=40
repeat-penalty=1.08
n-predict=600
system=You are a concise assistant.
color=no
```

Command line arguments take priority over the config file.

## Environment Variables

```text
SKIFFLLM_MODEL       Default model path
SKIFFLLM_MODEL_DIR   Default model directory
SKIFFLLM_CONFIG      Config file path
SKIFFLLM_HISTORY     History file path
SKIFFLLM_SYSTEM      Default system prompt
```

## Session History

Sessions are persisted in a compact binary format. The default file is:

```text
~/.local/share/skifflm/history.skif
```

The history is saved after each turn and when the program exits.

## Offline Guarantee

- No network access is required at runtime.
- No remote inference endpoints are used.
- No API keys are stored or requested.
- No telemetry, analytics, or crash reporting is included.
- All prompts and outputs stay on the local machine.

The only network use is optional and happens during the build when CMake downloads the pinned llama.cpp source tree. You can avoid that by supplying an existing llama.cpp checkout with `SKIFFLLM_LLAMA_SOURCE_DIR`.

## Project Layout

```text
CMakeLists.txt              Build configuration
include/skifflm/config.hpp  Configuration declarations
include/skifflm/engine.hpp  llama.cpp engine interface
include/skifflm/messages.hpp Chat message model
include/skifflm/session.hpp Session and history interface
include/skifflm/terminal.hpp Terminal UI interface
src/config.cpp              Configuration, argument and model discovery
src/engine.cpp              llama.cpp inference engine
src/session.cpp             Session persistence
src/terminal.cpp            Interactive terminal helpers
src/main.cpp                Application entry point
tests/test_main.cpp         Unit and integration tests
scripts/setup.sh            Build helper
```

## License

SkiffLLM is released under the MIT License.
