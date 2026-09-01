# Bekannte Grenzen

Diese Liste ist bewusst ehrlich. Sie beschreibt den aktuellen Stand des
Projekts.

## Modelldateien

SkiffLLM bündelt keine Modelle. Die Desktop-Laufzeitumgebung benötigt eine
bereits vorhandene GGUF-Datei. Dadurch bleibt die Netzwerknutzung der
Laufzeitumgebung bei null, aber der Einrichtungsaufwand ist höher als bei
Diensten mit eingebautem Abruf.

Es gibt zwei explizite Optionen:

- Desktop: `python3 scripts/model_fetch.py --model <id>`
- Android: `Settings` → `Models` → `Download`

Beide sind vom Benutzer ausgelöste HTTPS-Übertragungen von Hugging Face. Die
Android-App verwendet die Berechtigung `INTERNET` nur für diesen Zweck.

## Lokaler API-Server

Der `--serve`-Modus ist ein kompakter lokaler HTTP-Server. Er ist kein
multithreaded Produktions-Gateway.

- Schnelle Endpunkte (`/health`, `/version`, `/v1/models`) antworten, während
  eine Chat-Generierung läuft; die Generierung wird hinter einem Mutex
  serialisiert, weil ein llama.cpp-Kontext nicht threadsicher ist.
- `/v1/*`-Endpunkte akzeptieren optional `--api-key` und erfordern dann
  `Authorization: Bearer <key>`. Ohne Schlüssel sind sie öffentlich, was nur
  sicher ist, wenn auf `127.0.0.1` gelauscht wird.
- Es gibt kein Rate-Limiting.
- Er ist für lokale Werkzeuge, Editor-Plugins und persönliche Automatisierung
  gedacht.

## Kontext

Die Generierung ist durch die konfigurierte Kontextgröße begrenzt. Sehr lange
Unterhaltungen werden nur gekürzt, wenn `--auto-trim` aktiviert ist; andernfalls
wird ein Fehler erzeugt, wenn der Prompt zu groß ist.

## Prompt-Vorlagen

Die Chat-Vorlagen-Überschreibung akzeptiert einen Namen, der vom geladenen
llama.cpp-Modell unterstützt wird. Unbekannte Vorlagennamen fallen auf `chatml`
zurück oder schlagen mit einer klaren Fehlermeldung fehl.

## Android

- Führt jeweils eine Generierung und einen Download aus.
- Die App wärmt und lädt Modelle auf dem UI-ähnlichen Hintergrund-Executor;
  große Modelle benötigen weiterhin Zeit und Speicher.
- Modell-Downloads erfordern ausreichenden freien Speicherplatz und eine
  Netzwerkverbindung.
- GPU-Offload hängt von der llama.cpp-Build-Unterstützung und dem
  Geräte-Backend ab.

## CI

Die CI- und Release-Workflow-Dateien sind im Arbeitsbaum vorhanden, wurden aber
nicht zu diesem Branch gepusht, bis das Repository die erforderliche
`workflows`-Berechtigung gewährt. Bis dahin können lokale Prüfungen mit
`scripts/ci-local.sh` (Desktop) und `scripts/ci-android.sh` (Android, mit
installiertem Android SDK) ausgeführt werden.

## Geplant

- Embeddings und retrieval-gestützte Generierung
- Generierung mit mehreren Workern über mehrere Modelle
- Paketmanager-Integration
- Grammatik-beschränkte Generierung
- Backend-Benchmark-Matrix
