# SkiffLLM installieren

SkiffLLM ist eine einzelne, kleine C++-Binärdatei, die mit llama.cpp spricht.
Die Laufzeitumgebung bündelt kein Modell, greift nicht auf das Netzwerk zu und
benötigt keinen Daemon.

## 1. Fertiges Archiv installieren

Wenn eine Version veröffentlicht wird, enthält sie Plattformarchive namens
`skiffllm-<version>-<os>-<arch>.tar.gz` (unter Windows `.zip`) sowie
`checksums.txt`.

```bash
# Linux / macOS
tar -xzf skiffllm-v1.9.0-linux-x86_64.tar.gz
sudo install -m 0755 bin/skiffllm /usr/local/bin/skiffllm
```

Ein gebrauchsfertiges Hilfsskript ist enthalten:

```bash
bash scripts/install-from-release.sh --version v1.9.0
bash scripts/install-from-release.sh --help
```

Das Skript ordnet das aktuelle Betriebssystem und die Architektur dem
Release-Artefakt zu und installiert nach `$HOME/.local/bin`. Wenn das Archiv
noch nicht veröffentlicht ist, bricht das Skript sofort mit einer Fehlermeldung
ab.

## 2. Aus einem Quellcheckout installieren

### Schnelle lokale Installation

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

Nützliche Optionen:

```bash
bash scripts/install.sh --help                 # alle Optionen
bash scripts/install.sh --backend vulkan        # GPU-Backend-Build
bash scripts/install.sh --skip-tests            # nur bauen
```

### CMake-Installation

```bash
cmake --preset release
cmake --build build/release -j
cmake --install build/release --prefix /usr/local
```

## 3. Manueller Build

Voraussetzungen:

- CMake 3.20+
- Ein C++17-Compiler (GCC 10+, Clang 12+ oder MSVC 2019+)
- Ninja (empfohlen) oder Make
- Python 3 wird nur für das optionale Modellabruf-Hilfsskript benötigt

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Wenn Sie bereits einen gebauten und installierten llama.cpp-Checkout haben:

```bash
cmake -S . -B build \
  -DSKIFFLLM_FETCH_LLAMA=OFF \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DCMAKE_BUILD_TYPE=Release
```

## 4. Ein Modell besorgen

Die Laufzeitumgebung lädt niemals Modelle herunter.

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Dateien landen standardmäßig in `~/.local/share/skiffllm/models`. Sie können mit
`--model` auch auf jede vorhandene `.gguf`-Datei zeigen.

## 5. Shell-Vervollständigungen

Quellen Sie die generierten Dateien ein oder kopieren Sie sie:

```bash
# bash
source scripts/completions/skiffllm.bash

# zsh
cp scripts/completions/skiffllm.zsh ~/.zsh_functions/

# fish (Vervollständigungsverzeichnis)
cp scripts/completions/skiffllm.fish ~/.config/fish/completions/
```

## 6. macOS / iOS

- macOS wird über denselben CMake-Pfad gebaut; für Metal `--backend metal`
  verwenden.
- iOS erfordert macOS/Xcode + XcodeGen: `bash scripts/ios-setup.sh`.

## 7. Android

- Android Studio ist für diesen Installationsweg nicht erforderlich.
- `bash scripts/ci-android.sh` erzeugt ein Debug-APK, wenn das Android SDK
  installiert ist.

## 8. Demo aufzeichnen

Führen Sie `scripts/demo-capture.sh` mit einer echten Binärdatei und einem
echten GGUF-Modell aus, um eine ehrliche Terminalaufzeichnung zu erzeugen. Das
Skript erstellt niemals erfundene Ausgabe.

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /pfad/model.gguf
```
