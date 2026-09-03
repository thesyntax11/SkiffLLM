# Einrichtung

SkiffLLM hat keine Netzwerkabhängigkeiten zur Laufzeit. Sie bauen es einmal und
führen es gegen ein lokales GGUF-Modell aus.

## Voraussetzungen

- Ein C++17-Compiler (GCC 10+, Clang 12+ oder MSVC 2019+)
- CMake 3.20 oder neuer
- Optional: GNU Readline für Zeilenbearbeitung und Shell-Verlauf
- Optional: ein CUDA-, Metal-, ROCm-, SYCL- oder Vulkan-Backend für
  GPU-Offload
- Eine GGUF-Modelldatei

## Build

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

Oder verwenden Sie den bequemen Installer mit einem expliziten GPU/NPU-Backend:

```bash
bash scripts/install.sh                            # CPU / Plattform-Standard
BACKEND=cuda bash scripts/install.sh               # CUDA
BACKEND=vulkan bash scripts/install.sh             # Vulkan
BACKEND=metal bash scripts/install.sh              # macOS Metal
./build/llm --backend-info                     # verbundene Backends anzeigen
```

Verwenden Sie einen vorhandenen llama.cpp-Checkout, um den optionalen Download
zur Konfigurationszeit zu vermeiden:

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp --backend cuda
```

## Installation

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Ein Modell besorgen

Die SkiffLLM-Laufzeitumgebung lädt niemals ein Modell herunter. Sie können eine
vorhandene GGUF-Datei in das Modellverzeichnis kopieren oder den Pfad direkt
übergeben. Wenn Sie ein empfohlenes quantisiertes Modell möchten, führen Sie das
optionale Abruf-Hilfsskript einmal aus:

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Das Hilfsskript speichert die Datei in `~/.local/share/llm/models`. Die
Inferenz bleibt vollständig offline.

Oder kopieren Sie selbst:

```bash
mkdir -p ~/.local/share/llm/models
cp /path/to/model-q4_k_m.gguf ~/.local/share/llm/models/
```

Empfohlene kleine Modelle für einen schnellen CPU-only-Start:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

## Erster Lauf

```bash
./build/llm --doctor
./build/llm --model ~/.local/share/llm/models/model-q4_k_m.gguf --model-info
./build/llm --model ~/.local/share/llm/models/model-q4_k_m.gguf
```

## Tests ausführen

```bash
ctest --test-dir build --output-on-failure
```

Oder alle lokalen Prüfungen ausführen:

```bash
scripts/ci-local.sh
```

## Konfiguration

Die Standardkonfigurationsdatei liegt unter `~/.config/llm/config`. Ein
vollständiges Beispiel befindet sich in `configs/llm.example.conf`.
CLI-Flags überschreiben Werte aus der Konfigurationsdatei.

## Fehlerbehebung

- Wenn kein Modell gefunden wird, führen Sie `--list-models` aus oder übergeben
  Sie `--model`.
- Wenn die Generierung am Kontextlimit stoppt, erhöhen Sie `--ctx` oder
  verkürzen Sie die Konversation.
- Wenn ein fertiger llama.cpp verfügbar ist, setzen Sie
  `LLM_LLAMA_SOURCE_DIR`.
- Bei Systemen mit mehreren Sockets versuchen Sie `--numa`.
- Für schnellere CPU-Inferenz behalten Sie `--profile fast` und ein
  quantisiertes Modell bei.
