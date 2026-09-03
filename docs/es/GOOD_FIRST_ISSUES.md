# Buenas primeras tareas para colaboradores

Estos son buenos puntos de entrada para contribuir al proyecto. Cada elemento debe
conservar pruebas, documentación y paridad entre las tres plataformas
(escritorio/CLI, Android, iOS).

## Documentación

- Corrija traducciones faltantes o incorrectas en los documentos en español.
- Compare la paridad de contenido entre `docs/es/` y `docs/` e informe las
  diferencias.
- Compare los comandos de configuración y uso con la salida de `--help`.

## CLI

- Añada una prueba unitaria para una implementación de comando nuevo.
- Verifique la consistencia de los nombres de campo en las salidas `--json`.
- Añada mensajes de error más claros (por ejemplo, modelo faltante).

## Android

- Añada estados vacíos para campos `model.json` faltantes en la lista de modelos.
- Diseñe una pantalla pequeña que muestre los archivos de un modelo cargado.
- Mejore la retroalimentación para descargas canceladas.

## iOS

- Añada una página de prueba que compare la paridad de funciones con escritorio y
  Android.
- Divida `ChatView.swift` en componentes más pequeños.
- Añada una lista visual de sesiones guardadas.

## Calidad general

- Amplíe las comprobaciones en `scripts/ci-local.sh` y `scripts/ci-android.sh`.
- Añada una entrada de benchmark real a `docs/benchmarks.md` (solo generación real
  en su propio hardware).
- Verifique la tabla de funciones de README contra el comportamiento real.
