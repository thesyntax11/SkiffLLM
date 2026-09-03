# Limitaciones conocidas

Esta lista es intencionalmente honesta. Describe el estado actual del proyecto.

## Archivos de modelo

SkiffLLM no incluye modelos. La ejecución de escritorio requiere un archivo GGUF
ya existente, lo que mantiene el uso de red de la propia ejecución en cero y
aumenta el costo de configuración en comparación con los servicios que incluyen
recuperación.

Existen dos opciones explícitas:

- Escritorio: `python3 scripts/model_fetch.py --model <id>`
- Android: `Settings` → `Models` → `Download`

Ambas son transferencias HTTPS iniciadas por el usuario desde Hugging Face. La
aplicación Android usa el permiso `INTERNET` solo para este propósito.

## Servidor API local

El modo `--serve` es un servidor HTTP local compacto. No es una puerta de enlace
de producción con múltiples hilos.

- Los endpoints rápidos (`/health`, `/version`, `/v1/models`) responden mientras
  se ejecuta una generación de chat; la generación se serializa detrás de un
  mutex porque un contexto de llama.cpp no es seguro para subprocesos.
- Los endpoints `/v1/*` aceptan un `--api-key` opcional y luego requieren
  `Authorization: Bearer <key>`. Son públicos cuando no se establece una clave,
  lo cual es seguro solo al escuchar en `127.0.0.1`.
- No tiene limitación de velocidad.
- Está pensado para herramientas locales, complementos de editor y automatización
  personal.

## Contexto

La generación está limitada por el tamaño de contexto configurado. Las
conversaciones muy largas se recortan solo cuando `--auto-trim` está activado; de
lo contrario se produce un error cuando el prompt es demasiado grande.

## Plantillas de prompt

La anulación de la plantilla de chat acepta un nombre compatible con el modelo
llama.cpp cargado. Los nombres de plantilla desconocidos recurren a `chatml` o
fallan con un error claro.

## Android

- Ejecuta una generación a la vez y una descarga a la vez.
- La aplicación calienta y carga modelos en el ejecutor de fondo tipo UI; los
  modelos grandes aún requieren tiempo y memoria.
- Las descargas de modelos requieren suficiente almacenamiento libre y una
  conexión de red.
- La descarga en GPU depende del soporte de compilación de llama.cpp y del backend
  del dispositivo.

## CI

Los archivos de flujo de trabajo de CI y publicación están presentes en el árbol
de trabajo, pero no se han enviado a esta rama hasta que el repositorio otorgue el
permiso `workflows` requerido. Hasta entonces, las comprobaciones locales se
pueden ejecutar con `scripts/ci-local.sh` (escritorio) y `scripts/ci-android.sh`
(Android, con el SDK de Android instalado).

## Planificado

- Embeddings y generación aumentada por recuperación
- Generación con múltiples trabajadores entre varios modelos
- Integración con gestores de paquetes
- Generación restringida por gramática
- Matriz de benchmarks de backends
