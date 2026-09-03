## SkiffLLM

<p align="center">
  <img src="docs/logo.svg" alt="Logotipo de SkiffLLM" width="128" height="128"/>
</p>

<p align="center">
  <strong>Ejecuta cualquier modelo GGUF como una herramienta Unix.</strong><br/>
  Local. Sin conexión. Sin cuentas en la nube, sin telemetría, sin demonio.
</p>

<p align="center">
  <strong>Idiomas:</strong>
  <a href="README.md">English</a> ·
  <a href="README.tr.md">Türkçe</a> ·
  <a href="README.de.md">Deutsch</a> ·
  <a href="README.es.md">Español</a> ·
  <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="Licencia MIT"/>
  <img src="https://img.shields.io/badge/version-v1.6.0-blue" alt="Versión v1.6.0"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Plataformas"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Sin conexión"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/release.yml/badge.svg" alt="Release"/>
</p>

SkiffLLM es un único motor de IA local que se siente como una herramienta Unix.
Ejecuta cualquier modelo GGUF mediante llama.cpp en tu CPU o GPU, mantiene cada
token en tu máquina y se integra directamente en tu flujo de shell.

```bash
git diff | llm "review these changes"
cat error.log | llm "find the root cause"
cat README.md | llm "summarize this"
llm --project . "where is this implemented?"
```

---

## Primeros pasos

### Con un comando (desde el código fuente)

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
llm --version
```

### O compilar manualmente

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

### O instalar una versión publicada

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

El script asigna tu sistema operativo y arquitectura al archivo de la versión.
Falla rápidamente cuando la versión aún no tiene artefactos publicados.

### Grabar una demo

```bash
bash scripts/demo-capture.sh ./build/release/llm /path/to/model.gguf
```

Esto graba una sesión real de terminal para el README; nunca inventa salida.

### Conseguir un modelo pequeño

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Los archivos se guardan por defecto en `~/.local/share/llm/models`. También
puedes apuntar `--model` a cualquier archivo `.gguf` existente.

Los archivos de modelo se descargan de Hugging Face bajo su propia licencia
upstream; SkiffLLM no los redistribuye. Respeta la licencia de cada modelo
antes de usarlo o redistribuirlo. Licencias del catálogo incluido:
Qwen2.5/Qwen3 Apache-2.0, SmolLM2 Apache-2.0, Phi-3.5 MIT y Llama 3.2
**Llama 3.2 Community License** (con condiciones de atribución y nombres).

Configuración completa: [docs/es/INSTALL.md](docs/es/INSTALL.md).

---

## Por qué SkiffLLM

| | SkiffLLM | Asistentes en la nube | Ollama |
| --- | --- | --- | --- |
| Requiere nube | ❌ | ✅ | ❌ |
| Clave API en la nube | ❌ | ✅ | ❌ |
| Cuenta / registro | ❌ | ✅ | ❌ |
| Telemetría | ❌ | varía | ❌ |
| Ejecuta un demonio | ❌ | ✅ | ✅ |
| Archivo GGUF directo | ✅ | ❌ | parcial |
| Máquinas solo CPU | ✅ | ❌ | ✅ |
| Descarga a GPU | ✅ | n/a | ✅ |
| Tuberías Unix | ✅ | ❌ | ❌ |
| Contexto de proyecto/código | ✅ | ❌ | ❌ |
| API local compatible con OpenAI | ✅ | n/a | ✅ |
| Cliente Android nativo | ✅ | ❌ | comunidad |
| Cliente iOS nativo | ✅ | ❌ | comunidad |

El motor nunca descarga modelos, nunca envía datos al exterior y nunca necesita
una cuenta. Tú traes el archivo GGUF; SkiffLLM hace la inferencia.

### ¿Y por qué no simplemente Ollama?

Respuesta corta: usa Ollama cuando quieres un servidor de modelos rápido y
centrado en un catálogo con descargas en un solo comando; usa SkiffLLM cuando
quieres que el modelo se comporte como una herramienta Unix nativa, un
componente aislado de la red (air-gapped) o un motor incrustado en un flujo CI,
de escritorio o móvil: sin demonio, un único binario y una inferencia central
que nunca se conecta.
La matriz de decisión honesta, las compensaciones reales y una guía de
migración tarea a tarea están en [docs/es/comparison.md](docs/es/comparison.md)
y, completo en inglés, en [docs/COMPARISON.md](docs/COMPARISON.md). Para
despliegue empresarial, endurecimiento de servidor y cadena de suministro, ve
[docs/ENTERPRISE.md](docs/ENTERPRISE.md) (en inglés).

---

## Características destacadas

| Función | Descripción |
| --- | --- |
| Tuberías Unix | `cat file \| llm "summarize"`, `git diff \| llm "review"` |
| Contexto de proyecto | `--project <dir>` añade índice real + fragmento limitado de código |
| Gestor de modelos | `llm model list / info / install / remove / verify` |
| Integración Git | `llm git review / explain / commit / log / status` |
| Shell interactivo | Salida en streaming, historial, contadores en vivo |
| Contexto de archivo | `--attach`, `/file` y expansión `@file` en cualquier prompt |
| Exportación de conversación | `--export` y `/export` guardan sesiones en Markdown |
| Sesiones y memoria | sesiones con nombre, `/remember`, `/forget`, hechos persistentes |
| Servidor API local | `--serve` expone un endpoint compatible con OpenAI |
| Protección del servidor | autenticación Bearer opcional `--api-key` en `/v1/*` |
| Benchmark real | `--benchmark <runs>` mide velocidad real de prompt y generación |
| Perfiles de muestreo | `balanced`, `fast`, `creative`, `code`, `precise` |
| Muestreo avanzado | temperature, top-p, top-k, min-p, typical-p, penalizaciones |
| Gestión de contexto | recorte automático, espacio reservado, `/compact` |
| Modo JSON | salida legible por máquinas para scripts y herramientas |
| Diagnóstico | informe `--doctor`, `--model-info`, `--tokenize` |
| Apps móviles nativas | Android (Jetpack Compose) e iOS (SwiftUI) |

---

## Ejemplos prácticos

```bash
# Revisar un conjunto de cambios antes de subirlo
git diff | llm "review these changes"

# Encontrar la causa real en un log desordenado
journalctl -e | llm "find suspicious errors and a likely root cause"

# Resumir un archivo que acabas de leer
cat README.md | llm "summarize this"

# Apuntar a todo un repositorio
llm --project . "where is authentication implemented?"

# Salida legible por máquinas para tus propios scripts
git diff | llm --json "classify this diff"

# Revisión segura de código (propone diff, nunca edita archivos)
llm --code --project . "fix the bug in src/server.cpp"
```

## Gestor de modelos

SkiffLLM permanece sin conexión en tiempo de ejecución. Obtener un modelo es un
comando explícito y separado.

```bash
llm model list
llm model info qwen2.5-0.5b
llm model install qwen2.5-0.5b
llm model verify qwen2.5-0.5b --update
llm model remove qwen2.5-0.5b --force
```

`model install` delega en `scripts/model_fetch.py`, que descarga exactamente un
GGUF por HTTPS desde Hugging Face, comprueba el encabezado GGUF y guarda un
archivo lateral SHA-256 en tu directorio de modelos. El tamaño del catálogo es
solo orientativo porque un mantenedor puede volver a subir una revisión con
otro tamaño; el lateral es la comprobación de integridad autoritativa. La
inferencia en sí nunca abre una conexión.

## Integración con Git

Revisión y explicación de código local y sin conexión para el diff que tienes
delante.

```bash
# Los subcomandos git leen el diff ellos mismos; no se necesita una tubería.
llm git review
llm git review --cached
llm git explain
llm git commit --cached
llm git log
llm git status
```

`git commit --cached` propone un mensaje de commit convencional a partir de tu
diff en stage; no ejecuta `git commit` por ti.

## Sesiones y memoria persistente

```bash
llm --session coding --model qwen2.5-0.5b-instruct-q4_k_m.gguf
llm --session writing --model qwen2.5-0.5b-instruct-q4_k_m.gguf

llm session list
llm session show coding
llm session rename coding writing
llm session remove old-draft
```

La memoria persistente vive en `~/.local/share/llm/memories.txt` y nunca
abandona la máquina.

```bash
llm --remember "the user prefers concise answers"
llm --forget concise
```

Dentro del shell interactivo usa `/remember`, `/forget`, `/memories`,
`/clear-memories`, `/compact`, `/regenerate` y `/export`.

---

## Servidor local compatible con OpenAI

```bash
# solo local
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# listener no local protegido por un token compartido
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$LLM_SERVER_KEY"
```

Endpoints:

```text
GET  /health                 comprobación de salud pública
GET  /version                comprobación de versión pública
GET  /v1/models              protegido por Bearer si --api-key está configurado
POST /v1/chat/completions    protegido por Bearer si --api-key está configurado
```

El servidor admite streaming estilo OpenAI (`"stream": true`) y responde a los
endpoints rápidos mientras se ejecuta una generación. La generación de chat está
serializada porque un contexto de llama.cpp no es seguro entre hilos.

```python
from openai import OpenAI

client = OpenAI(base_url="http://127.0.0.1:8080/v1", api_key="local-token")
resp = client.chat.completions.create(
    model="qwen2.5-0.5b-instruct-q4_k_m",
    messages=[{"role": "user", "content": "Explain this diff."}],
    stream=True,
)
for chunk in resp:
    print(chunk.choices[0].delta.content or "", end="")
```

Se incluye un cliente Python sin dependencias:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$LLM_SERVER_KEY" "Say hello."
```

---

## Apps móviles

SkiffLLM incluye clientes nativos para [Android](android/README.md) e
[iOS](ios/README.md). Ambos ejecutan modelos GGUF compatibles completamente en
el dispositivo, transmiten tokens, muestran una barra de uso de contexto en vivo
y exponen la misma superficie de funciones que la CLI de escritorio:

- configuración de muestreo y modo código por conversación
- creación/apertura/renombrado/borrado/copia de seguridad/importación múltiple
- hechos persistentes, prompts rápidos e ingreso desde el panel de compartir
- secuencias de parada, perfiles de muestreo y compactación de conversaciones
- calentamiento del modelo, benchmark real de 3 rondas, estadísticas de sesión
- adjuntar archivos de texto/JSON/XML (leídos localmente, limitados)
- exportación Markdown y copia con un toque
- importación GGUF con verificación del encabezado

La inferencia nunca abre una conexión. Hay exactamente dos rutas de red, ambas
explícitas e iniciadas por el usuario: `model install` descarga desde Hugging
Face, y el subcomando `openai` habla con un servidor HTTP al que lo apuntes. El
archivo debe tener un encabezado GGUF válido y recibe un archivo lateral
SHA-256; el tamaño del catálogo es orientativo. Las copias de seguridad de
Android están desactivadas para que los prompts y el historial nunca salgan
del dispositivo.

---

## Línea de comandos

```text
Usage: llm [options] [model.gguf]

Core options:
  --model <path>             Path to a GGUF model file
  --model-dir <path>         Directory scanned for a GGUF model
  --list-models              Print discovered GGUF models
  --model-info               Print model metadata and exit
  --smoke                    Run a quick generation smoke test
  --warmup                   Warm the model before the first answer
  --doctor                   Print system diagnostics
  --tokenize <text>          Tokenize text and print token counts
  --profile <name>           balanced, fast, creative, code or precise
  --session <name>           Use a named conversation
  --system <text>            System prompt
  --stop <text>              Stop sequence; can be repeated
  --attach <path>            Attach a file; can be repeated
  --file <path>             Alias for --attach; can be repeated
  --chat-template <name>     Override the model chat template
  --export <path>            Export the loaded conversation as Markdown
  --serve                    Serve a local OpenAI-compatible API
  --host <addr>              Local server bind address (default: 127.0.0.1)
  --port <n>                 Local server port (default: 8080)
  --api-key <key>            Require Bearer auth on the local server
  --benchmark <runs>         Run a real generation benchmark

Subcommands:
  run [prompt] [opts]        One-shot prompt
  model list|info|install|remove|verify
  chat-template list|detect|info
  openai [prompt] [opts]     Send a prompt to a local OpenAI-compatible server
  config path|show|init      Manage the config file
  server health [--json]     Check a running local server
```

Uso completo: [docs/es/usage.md](docs/es/usage.md). Comandos interactivos:
`/help`, `/warmup`, `/history`, `/stats`, `/compact`, `/regenerate`,
`/tokenize`, `/file`, `/clear-attach`, `/clear`, `/reset`, `/system`,
`/model`, `/profile`, `/stop`, `/temp`, `/top-p`, `/top-k`, `/min-p`,
`/typical`, `/n`, `/ctx`, `/export`, `/save`, `/exit`.

---

## Honestidad en benchmarks

```bash
llm --model model.gguf --benchmark 3
llm --model model.gguf --benchmark 3 --json
```

Cada número se mide en tu máquina con tu modelo y tu hardware. SkiffLLM nunca
inventa resultados de benchmark. Metodología y tabla vacía esperando aportes
reales: [docs/benchmarks.md](docs/benchmarks.md).

## Prueba de privacidad

```bash
llm --doctor --network
```

imprime los hechos del runtime: la generación central no hace ninguna llamada
saliente, no hay telemetría ni API en la nube, y el historial se guarda
localmente. Las únicas rutas de red son explícitas y las inicia el usuario:
`model install` (descarga de Hugging Face) y el subcomando `openai` (el
servidor al que lo apuntes). `--serve` solo abre un listener local, nunca
marca hacia fuera.

---

## Instalación

```bash
bash scripts/install.sh --help
bash scripts/install.sh --prefix "$HOME/.local"
bash scripts/install.sh --prefix /usr/local --backend metal
```

`Makefile` de conveniencia:

```bash
make release
make tests
make check
make install
make help
```

Los archivos precompilados siguen el patrón `llm-<version>-<os>-<arch>.tar.gz`
(por ejemplo `llm-v1.6.0-linux-x86_64.tar.gz`, `.zip` en Windows) con
`checksums.txt` cuando se publican. Ver [docs/es/INSTALL.md](docs/es/INSTALL.md).
Los completions de shell están en [scripts/completions](scripts/completions/).

## Opciones de compilación

| Opción | Por defecto | Descripción |
| --- | --- | --- |
| `LLM_BUILD_TESTS` | `ON` | Compilar y registrar la suite de pruebas |
| `LLM_FETCH_LLAMA` | `ON` | Descargar y compilar llama.cpp fijado |
| `LLM_LLAMA_SOURCE_DIR` | vacío | Usar un checkout existente de llama.cpp |
| `LLM_BUILD_SHARED_LLAMA` | `OFF` | Compilar llama.cpp como biblioteca compartida |
| `LLM_USE_READLINE` | `ON` | Activar GNU Readline cuando esté disponible |
| `LLM_LLAMA_BACKEND` | `auto` | `cuda`, `metal`, `vulkan`, `opencl`, `blas`, `cpu` |

La aceleración por hardware siempre es explícita: elige el backend al
configurar y descarga capas en tiempo de ejecución con `--gpu-layers`.

---

## Requisitos

- Compilador C++17 (GCC 10+, Clang 12+, o MSVC 2019+)
- CMake 3.20+
- `FetchContent` de CMake opcional para un llama.cpp fijado
- Un archivo de modelo GGUF
- Opcional: CUDA/Metal/Vulkan/OpenCL/BLAS para aceleración por hardware

---

## Estructura del proyecto

```text
include/llm/              Encabezados de API pública
src/                          CLI, núcleo y servidor local
tests/                        Pruebas unitarias (sin modelo)
configs/                      Configuración de ejemplo
scripts/                      CI, releases, descarga de modelos, cliente API, completions
android/                      App Android Kotlin/Compose y JNI de llama.cpp
ios/                          App iOS SwiftUI y puente Objective-C++ de llama.cpp
docs/                         Setup, uso, arquitectura, FAQ, docs de release
.github/                      Plantillas de issues, plantilla de PR, workflow de CI
```

---

## Hoja de ruta

- Modo de embeddings y RAG
- Integración con gestores de paquetes (Homebrew, apt, vcpkg)
- Matriz de benchmarks GPU entre backends
- Generación restringida por gramática
- Diagnóstico de muestreo a nivel de token
- Generación multi-trabajador entre varios modelos

## Limitaciones conocidas

SkiffLLM no incluye modelos. El servidor local es público en `127.0.0.1` por
defecto; usa `--api-key` al vincular a una interfaz no loopback. Ver
[docs/es/LIMITATIONS.md](docs/es/LIMITATIONS.md).

## Contribuciones

Las contribuciones son bienvenidas. Mantén los cambios pequeños, respeta la
promesa offline, nunca inventes números de benchmark y añade pruebas para
comportamientos nuevos. Ver [CONTRIBUTING.md](CONTRIBUTING.md) y
[docs/es/GOOD_FIRST_ISSUES.md](docs/es/GOOD_FIRST_ISSUES.md). Guía de traducción:
[docs/i18n.md](docs/i18n.md).

## Licencia

MIT — ver [LICENSE](LICENSE).
