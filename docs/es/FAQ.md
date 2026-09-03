# Preguntas frecuentes

## ¿SkiffLLM sube mis conversaciones?

No. La ejecución de escritorio no tiene código de red. Los prompts, el historial,
la configuración y el texto generado permanecen en la máquina. La aplicación
Android no envía ningún prompt a ninguna parte.

## ¿SkiffLLM descarga modelos?

No automáticamente. La ejecución de escritorio requiere un archivo GGUF. Hay dos
asistentes opcionales disponibles:

- Escritorio: `python3 scripts/model_fetch.py --model qwen2.5-0.5b`
- Android: `Settings` → `Models` → `Download`

Ambos usan HTTPS desde Hugging Face y son acciones explícitas del usuario.

## ¿Qué modelo debería usar?

Empiece con un GGUF instruct pequeño en formato Q4_K_M:

- Qwen2.5-0.5B-Instruct
- Qwen3-0.6B-Instruct
- Llama-3.2-1B-Instruct

En un teléfono con menos de 4 GB de RAM, use los modelos de 0.5B o 0.6B.

## ¿Por qué mi primera respuesta es lenta?

La primera pasada después de cargar el modelo incluye el procesamiento del prompt
y el calentamiento de fallos de página. Use `--warmup` en escritorio o inicie la
aplicación Android una vez antes de una conversación real. La aplicación Android
calienta el modelo automáticamente después de cargarlo.

## ¿Dónde se almacenan las sesiones?

Escritorio: el directorio de sesiones de `--session`/`--history` (por defecto bajo
`~/.local/share/llm`). Android: `conversation.json` interno de la app; borrar
los datos de la app lo elimina.

## ¿Cómo expongo la API local?

```bash
# solo local
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# accesible desde otra máquina, protegida por un token compartido
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "local-token"
```

Con `--api-key` configurado, `/v1/models` y `/v1/chat/completions` requieren
`Authorization: Bearer <key>` y en caso contrario devuelven `401`.
`/health`, `/version` y `/` permanecen públicos. Mantenga el enlace predeterminado
`127.0.0.1` siempre que sea posible; los listeners no loopback siempre deben
configurar `--api-key`.

## ¿Por qué `--benchmark` difiere de los números de los proveedores?

Cada benchmark en SkiffLLM se mide en su máquina, con el archivo de modelo y el
hardware que usted proporciona. Los números dependen de CPU/GPU, cuantización,
tamaño de contexto, hilos y carga del sistema. No hay números falsos ni de
marketing.

## ¿La aplicación Android funciona sin conexión de red?

Sí, después de cargar un modelo. Cargar un modelo guardado, chatear, exportar y
borrar funcionan todos sin conexión. La red solo se usa si elige descargar un
modelo nuevo desde Hugging Face.

## ¿La telemetría está habilitada?

No. No hay analíticas, informes de fallos ni seguimiento de uso en la ejecución de
escritorio ni en la aplicación Android.
