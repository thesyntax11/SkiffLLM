# ¿Por qué SkiffLLM y no simplemente Ollama?

Esta es la pregunta que hace cada usuario nuevo y merece una respuesta directa y
honesta. SkiffLLM y Ollama resuelven problemas superpuestos; la elección
correcta depende de tu modelo de despliegue.

Resumen: **Usa Ollama cuando quieras un servidor de modelos ágil con un catálogo
grande y descargas en un solo comando. Usa SkiffLLM cuando quieras que el modelo
se comporte como una herramienta Unix nativa, un componente aislado de la red
(air-gapped) o un motor de inferencia incrustado en un flujo de CI, escritorio o
móvil, sin demonio y sin red en tiempo de ejecución.**

## Matriz de decisión

| Dimensión | SkiffLLM | Ollama |
| --- | --- | --- |
| Uso de red en ejecución | ninguno en la inferencia | servicio de descarga/pull por defecto |
| Demonio / servicio en segundo plano | ninguno (un solo proceso) | sí (`ollama serve`) |
| Archivo de modelo | traes tu GGUF | catálogo gestionado, auto-pull |
| Fijación / integridad | sidecar SHA-256 + `model verify` | referencias de registro, menos explícito |
| Uso primero en Unix | pipelines nativas, `--project`, subcomandos `git` | servidor + cliente, no pipe-first |
| Contexto de proyecto/código | índice de archivos integrado + porción de código | vía herramientas externas, no integrado |
| Clientes móviles nativos | Android + iOS en este repo | orientado a servidor, apps comunitarias |
| Aislado / offline-first | objetivo explícito | configurable, no postura por defecto |
| API compatible con OpenAI | `--serve` (streaming, auth Bearer) | sí, modelo principal |
| Control del motor | tu build controla los backends de llama.cpp | runtime empaquetado |
| Huella | un binario pequeño | demonio + runtime + almacén de modelos |
| Amplitud de catálogo | eliges cualquier GGUF | amplio, cómodo |
| Ecosistema / GUIs (Open WebUI etc.) | DIY | maduro |
| Límites duros del servidor | una generación, sin límite de tasa interno | escala más fácilmente, mayor huella |

Ninguna fila es un defecto por sí misma. La tabla pretende eliminar la duda.

## Dónde SkiffLLM es claramente mejor

### 1. Quieres una herramienta Unix, no un servicio

Ollama es server-first: ejecutas un servicio y hablas con una API HTTP. SkiffLLM
es CLI-first:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
skifflm --project . "where is authentication handled?"
skifflm --code --project . "propose a fix for src/server.cpp"
```

No hay proceso en segundo plano, puerto que gestionar ni contenedor con control
de residencia. Se compone con `jq`, `xargs`, `git` y cron como cualquier otra
herramienta.

### 2. Estás aislado de la red u offline-first

El runtime no descarga modelos y no tiene código de red. Tú traes un archivo
GGUF; SkiffLLM hace la inferencia. Es una postura explícita, no un ajuste que
debas recordar. Para redes restringidas o aisladas es la diferencia entre
"funciona por defecto" y "funciona solo después de desactivar la red".

### 3. Necesitas control sobre el motor de inferencia

SkiffLLM se compila contra el llama.cpp que elijas. El backend se decide al
configurar `SKIFFLLM_LLAMA_SOURCE_DIR` y el flag de backend de build;
`--backend-info` te dice qué está realmente enlazado. Tú posees el compilador,
el backend y el binario. Ollama empaqueta y gestiona su runtime; cómodo pero
menos transparente.

### 4. Quieres evidencia de la cadena de suministro

`skifflm model verify` comprueba la cabecera mágica GGUF, el tamaño esperado y un
sidecar SHA-256. `model_fetch.py --checksum` registra el sidecar y `--verify`
comprueba una descarga existente sin volver a descargar. Es el tipo de evidencia
que quiere una auditoría: qué modelo, qué hash, de dónde, qué se midió.

### 5. Quieres paridad móvil desde el mismo motor

El repo incluye clientes Android y iOS nativos contra los mismos archivos de
modelo y la misma promesa offline. Esto no es una función de Ollama; existen
proyectos móviles comunitarios pero no forman parte del proyecto principal.

### 6. Quieres benchmarks reproducibles y honestos

`--benchmark` ejecuta generaciones reales en tu máquina y reporta el tiempo de
prompt, el tiempo de generación y tokens/s medidos. Los docs piden
explícitamente la salida del comando y el SHA-256 del modelo antes de aceptar un
resultado. No hay cifras de marketing.

## Dónde Ollama es mejor

- **Configuración de modelo en un comando.** `ollama pull llama3` es más simple
  que buscar, descargar y verificar un GGUF manualmente.
- **Amplitud de catálogo.** La biblioteca oficial de modelos es mucho más grande
  y fácil de explorar que una búsqueda manual de GGUF.
- **Cargas server-first.** Si la interfaz principal es HTTP, el modelo de daemon
  está bien y el ecosistema de Open WebUI es maduro.
- **No necesitas un modelo de objetos estilo Unix.** Si la tarea es "darle al
  equipo una caja de chat local", Ollama es el camino de menor fricción.
- **Concurrencia multi-request.** El servidor de Ollama está construido para
  atender muchos clientes. SkiffLLM serializa intencionalmente la generación
  detrás de un mutex porque un contexto de llama.cpp no es thread-safe.

## Advertencias honestas sobre SkiffLLM

- No incluye modelos; el costo de configuración es mayor que un catálogo
  gestionado.
- El modo `--serve` es un servidor local compacto, no una puerta de enlace
  multihilo: una generación a la vez, sin límite de tasa interno, token Bearer
  compartido opcional.
- Aún no hay un gran ecosistema de GUIs.
- Debes verificar que el backend de tu build de llama.cpp soporte tu hardware
  GPU.

## Recomendación

| Situación | Usar |
| --- | --- |
| Shell-first, CI, revisión git, portátiles offline | SkiffLLM |
| Aislado de la red / red restringida | SkiffLLM |
| Incrustado en flujo de escritorio/móvil | SkiffLLM |
| Fijación de cadena de suministro y benchmarks reproducibles | SkiffLLM |
| Caja de chat rápida con gran catálogo | Ollama |
| Servicio HTTP-first con concurrencia | Ollama |

Elige honestamente, no por costumbre. Si comparas, parte de tu modelo de
despliegue y no de una lista de funciones. Para producción ve
[docs/ENTERPRISE.md](../ENTERPRISE.md) y para convertir hábitos de Ollama
[docs/OLLAMA_MIGRATION.md](../OLLAMA_MIGRATION.md) (en inglés).
