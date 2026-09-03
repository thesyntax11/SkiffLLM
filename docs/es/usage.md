# Guía de uso

## Modo de canalización Unix

SkiffLLM detecta automáticamente stdin canalizado, por lo que el modelo forma
parte de un flujo de trabajo de shell:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

Sin argumento de instrucción, el texto canalizado es el propio prompt. Con un
argumento de instrucción, el texto canalizado se convierte en `<context>`.

## Contexto de proyecto

```bash
skifflm --project . "where is authentication handled?"
```

`--project <dir>` construye un índice de archivos limitado más una porción del
contenido de código/configuración antes de la generación. Omite `.git`,
directorios de compilación, cachés y dependencias empaquetadas.

## Sesiones y memoria

```bash
skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft

skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Comandos de shell interactivos: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (comprimir una conversación larga en
un resumen con viñetas conservando los datos) y `/regenerate` o `/retry`
(re-ejecutar el último mensaje del usuario con la configuración de muestreo
actual).

## Atajo de resumen

```bash
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Gestor de modelos

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model verify` comprueba la cabecera mágica GGUF y el archivo complementario
SHA-256 cuando existe. El tamaño del catálogo es orientativo; una revisión
upstream más nueva no se rechaza solo por cambiar el número de bytes. `--update` registra un archivo
complementario SHA-256 local. Las descargas de `model_fetch.py` también escriben
ese complemento automáticamente y admiten `--verify` sin volver a descargar.

La inferencia permanece sin conexión. `model install` ejecuta el asistente
explícito `model_fetch.py` por HTTPS.

## Ejecución única y plantillas de chat

```bash
skifflm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skifflm chat-template list
skifflm chat-template detect --model model.gguf
```

## Cliente OpenAI

```bash
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Aceleración por hardware

```bash
skifflm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Los backends se eligen en tiempo de compilación; `--backend-info` informa lo que
realmente está enlazado.

## Integración con Git

```bash
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

## Modo de código seguro

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` produce una propuesta de diff unificado y nunca edita un archivo por sí
mismo.

## Modo interactivo

```bash
skifflm --model ~/models/model-q4_k_m.gguf
```

La interfaz es un prompt llamado `you>`. Escriba un mensaje y presione Enter. El
texto se transmite mientras se genera.

## Modo de una sola ejecución

```bash
skifflm --model model.gguf --prompt "What is recursion?"
```

## Sesiones con nombre

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

Cada sesión con nombre obtiene su propio archivo de historial.

## Prompt de sistema

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Perfiles

```bash
skifflm --model model.gguf --profile code
```

Perfiles disponibles: `balanced`, `fast`, `creative`, `code`, `precise`.

## Contexto de archivo

```bash
skifflm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skifflm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Repita `--attach`, use `@path` en el prompt o administre los adjuntos dentro del
shell con `/file` y `/clear-attach`.

## Exportar conversación

```bash
skifflm --export conversation.md
```

Exportar la sesión cargada no necesita un modelo y escribe Markdown. Dentro del
shell use `/export <path>`.

## Plantilla de chat y calentamiento

```bash
skifflm --model model.gguf --chat-template chatml
skifflm --model model.gguf --warmup
```

`--chat-template` anula el nombre del formato de prompt integrado del modelo.
`--warmup` ejecuta una generación de un token al inicio para reducir la latencia
de la primera respuesta. Dentro del shell también puede volver a calentar el
modelo con `/warmup`.

## Edición de línea interactiva

Cuando GNU Readline está disponible, SkiffLLM habilita la navegación con flechas,
la búsqueda en el historial y líneas de entrada editables. Sin Readline recurre a
`getline` simple. La salida en streaming también muestra un contador de tokens en
vivo en terminales interactivas.

## Secuencias de parada

```bash
skifflm --model model.gguf --stop "END" --stop "STOP"
```

La generación se detiene en la primera secuencia configurada.

## Modo JSON

```bash
skifflm --model model.gguf --prompt "Say hello" --json
```

Esto desactiva el shell interactivo y escribe un único objeto JSON en stdout.

## Tuberías

```bash
cat prompt.txt | skifflm --model model.gguf
printf 'Explain this command.' | skifflm --model model.gguf --prompt-file /dev/stdin
```

## Archivos de salida

```bash
skifflm --model model.gguf --prompt-file input.txt --output output.md
```

## Archivo de configuración

```bash
skifflm --config ~/.config/skifflm/config
```

La ubicación predeterminada se usa automáticamente cuando existe.

## Diagnóstico

```bash
skifflm --doctor
skifflm --model model.gguf --model-info
skifflm --model model.gguf --smoke
skifflm --model model.gguf --tokenize "hello world"
```

## Servidor API local

```bash
# solo local
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# listener no loopback protegido
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Endpoints:

```text
GET  /health                 comprobación de salud pública
GET  /v1/models              protegido por bearer cuando --api-key está configurado
POST /v1/chat/completions    protegido por bearer cuando --api-key está configurado
```

El servidor está sin conexión, se enlaza a localhost por defecto y admite
streaming estilo OpenAI con `"stream": true`. Los endpoints rápidos responden
mientras se ejecuta una generación; la generación de chat se serializa detrás de
un mutex. Cuando `--api-key` está configurado, `/v1/*` requiere
`Authorization: Bearer <key>` y devuelve `401` en caso contrario.

Se incluye un cliente rápido:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Ejecuta una generación real e informa el tiempo de prompt medido, el tiempo de
generación y los tokens por segundo. `--n-predict` controla la longitud de la
ejecución hasta 128 tokens por ejecución.

## Descarga en GPU

En una máquina CUDA:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skifflm --model model.gguf --gpu-layers -1
```

En macOS, el backend Metal está disponible por defecto.

## Listado de modelos

```bash
skifflm --model-dir ~/models --list-models
```
