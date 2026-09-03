# Instalación de SkiffLLM

SkiffLLM es un binario C++ único y pequeño que se comunica con llama.cpp. La
ejecución no incluye un modelo, no llama a la red y no necesita un demonio.

## 1. Instalar un archivo precompilado

Cuando se publica una versión, incluye archivos de plataforma llamados
`skiffllm-<version>-<os>-<arch>.tar.gz` (o `.zip` en Windows) además de
`checksums.txt`.

```bash
# Linux / macOS
tar -xzf skiffllm-v1.6.0-linux-x86_64.tar.gz
sudo install -m 0755 bin/skiffllm /usr/local/bin/skiffllm
```

Se incluye un asistente listo para usar:

```bash
bash scripts/install-from-release.sh --version v1.6.0
bash scripts/install-from-release.sh --help
```

El asistente asigna el SO/arquitectura actual al recurso de la versión y
instala en `$HOME/.local/bin`. Falla rápidamente si el archivo aún no se ha
publicado.

## 2. Instalar desde una copia del código fuente

### Instalación local rápida

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

Opciones útiles:

```bash
bash scripts/install.sh --help                 # todas las opciones
bash scripts/install.sh --backend vulkan        # compilación con backend GPU
bash scripts/install.sh --skip-tests            # solo compilar
```

### Instalación con CMake

```bash
cmake --preset release
cmake --build build/release -j
cmake --install build/release --prefix /usr/local
```

## 3. Compilación manual

Requisitos:

- CMake 3.20+
- Un compilador C++17 (GCC 10+, Clang 12+ o MSVC 2019+)
- Ninja (recomendado) o Make
- Python 3 solo se necesita para el asistente opcional de descarga de modelos

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Si ya tiene una copia de llama.cpp compilada e instalada:

```bash
cmake -S . -B build \
  -DSKIFFLLM_FETCH_LLAMA=OFF \
  -DSKIFFLLM_LLAMA_SOURCE_DIR=/path/to/llama.cpp \
  -DCMAKE_BUILD_TYPE=Release
```

## 4. Obtener un modelo

La ejecución nunca descarga modelos.

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Los archivos se guardan en `~/.local/share/skiffllm/models` por defecto. También
puede apuntar `--model` a cualquier `.gguf` existente.

## 5. Completado de shell

Copie o cargue los archivos generados:

```bash
# bash
source scripts/completions/skiffllm.bash

# zsh
cp scripts/completions/skiffllm.zsh ~/.zsh_functions/

# fish (directorio de completados)
cp scripts/completions/skiffllm.fish ~/.config/fish/completions/
```

## 6. macOS / iOS

- macOS se compila por la misma ruta de CMake; use `--backend metal` para Metal.
- iOS requiere macOS/Xcode + XcodeGen: `bash scripts/ios-setup.sh`.

## 7. Android

- Android Studio no es necesario en esta ruta de instalación.
- `bash scripts/ci-android.sh` produce un APK de depuración cuando el Android
  SDK está instalado.

## 8. Grabar una demo

Ejecute `scripts/demo-capture.sh` con un binario real y un modelo GGUF real para
producir una grabación de terminal honesta. El script nunca crea salida falsa.

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /ruta/model.gguf
```
