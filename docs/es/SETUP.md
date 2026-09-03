# Configuración

SkiffLLM no tiene dependencias de red en tiempo de ejecución. Se compila una vez y
se ejecuta contra un modelo GGUF local.

## Requisitos

- Un compilador C++17 (GCC 10+, Clang 12+ o MSVC 2019+)
- CMake 3.20 o más reciente
- Opcional: GNU Readline para edición de línea e historial de shell
- Opcional: un backend CUDA, Metal, ROCm, SYCL o Vulkan para descarga en GPU
- Un archivo de modelo GGUF

## Compilar

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

O use el instalador de conveniencia con un backend GPU/NPU explícito:

```bash
bash scripts/install.sh                            # CPU / predicción de plataforma
BACKEND=cuda bash scripts/install.sh               # CUDA
BACKEND=vulkan bash scripts/install.sh             # Vulkan
BACKEND=metal bash scripts/install.sh              # macOS Metal
./build/skifflm --backend-info                     # inspeccionar backends enlazados
```

Use una copia existente de llama.cpp para evitar la descarga opcional en tiempo de
configuración:

```bash
scripts/setup.sh --source-dir /path/to/llama.cpp --backend cuda
```

## Instalar

```bash
scripts/setup.sh --prefix /usr/local
cmake --install build --prefix /usr/local
```

## Obtener un modelo

La ejecución de SkiffLLM nunca descarga un modelo. Puede copiar un archivo GGUF que
ya tenga en el directorio de modelos o pasar su ruta directamente. Si quiere un
modelo cuantizado recomendado, ejecute el asistente de descarga opcional una vez:

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

El asistente guarda el archivo en `~/.local/share/skifflm/models`. La inferencia
sigue siendo totalmente sin conexión.

O cópielo usted mismo:

```bash
mkdir -p ~/.local/share/skifflm/models
cp /path/to/model-q4_k_m.gguf ~/.local/share/skifflm/models/
```

Modelos pequeños recomendados para un inicio rápido solo con CPU:

- Qwen2.5-0.5B-Instruct GGUF
- Qwen2.5-1.5B-Instruct GGUF
- Llama-3.2-1B-Instruct GGUF
- Phi-3.5-mini-instruct GGUF
- SmolLM2-1.7B-Instruct GGUF

## Primera ejecución

```bash
./build/skifflm --doctor
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf --model-info
./build/skifflm --model ~/.local/share/skifflm/models/model-q4_k_m.gguf
```

## Ejecutar las pruebas

```bash
ctest --test-dir build --output-on-failure
```

O ejecute todas las comprobaciones locales:

```bash
scripts/ci-local.sh
```

## Configuración

El archivo de configuración predeterminado es `~/.config/skifflm/config`. Un
ejemplo completo está en `configs/skifflm.example.conf`. Los indicadores de CLI
anulan los valores del archivo de configuración.

## Solución de problemas

- Si no se encuentra ningún modelo, ejecute `--list-models` o pase `--model`.
- Si la generación se detiene en el límite de contexto, suba `--ctx` o acorte la
  conversación.
- Si hay un llama.cpp precompilado disponible, configure
  `SKIFFLLM_LLAMA_SOURCE_DIR`.
- En sistemas con varios sockets, pruebe `--numa`.
- Para una inferencia de CPU más rápida, mantenga `--profile fast` y un modelo
  cuantizado.
