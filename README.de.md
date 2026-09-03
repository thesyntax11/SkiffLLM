## SkiffLLM

<p align="center">
  <img src="docs/logo.svg" alt="SkiffLLM-Logo" width="128" height="128"/>
</p>

<p align="center">
  <strong>Jedes GGUF-Modell wie ein Unix-Werkzeug ausführen.</strong><br/>
  Lokal. Offline. Keine Cloud-Konten, keine Telemetrie, kein Daemon.
</p>

<p align="center">
  <strong>Sprachen:</strong>
  <a href="README.md">English</a> ·
  <a href="README.tr.md">Türkçe</a> ·
  <a href="README.de.md">Deutsch</a> ·
  <a href="README.es.md">Español</a> ·
  <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License MIT"/>
  <img src="https://img.shields.io/badge/version-v1.6.0-blue" alt="Version v1.6.0"/>
  <img src="https://img.shields.io/badge/c%2B%2B-17-blue.svg" alt="C++17"/>
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20Android%20%7C%20iOS-lightgrey" alt="Plattformen"/>
  <img src="https://img.shields.io/badge/runtime-offline-green" alt="Offline"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/ci.yml/badge.svg" alt="CI"/>
  <img src="https://github.com/thesyntax11/SkiffLLM/actions/workflows/release.yml/badge.svg" alt="Release"/>
</p>

SkiffLLM ist eine einzelne lokale KI-Laufzeit, die sich wie ein Unix-Werkzeug
anfühlt. Sie führt jedes GGUF-Modell über llama.cpp auf Ihrer CPU oder GPU aus,
behält jedes Token auf Ihrem Rechner und fügt sich direkt in Ihren
Shell-Workflow ein.

```bash
git diff | skiffllm "review these changes"
cat error.log | skiffllm "find the root cause"
cat README.md | skiffllm "summarize this"
skiffllm --project . "where is this implemented?"
```

---

## Erste Schritte

### Mit einem Befehl (aus dem Quellcode)

```bash
git clone https://github.com/thesyntax11/SkiffLLM.git
cd SkiffLLM
bash scripts/install.sh --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
skiffllm --version
```

### Oder manuell bauen

```bash
cmake --preset release
cmake --build build/release -j
ctest --test-dir build/release --output-on-failure
```

### Oder eine veröffentlichte Version installieren

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

Das Skript ordnet Ihr Betriebssystem und Ihre Architektur dem Release-Archiv
zu. Es bricht schnell ab, wenn das Release noch keine Artefakte enthält.

### Demo aufzeichnen

```bash
bash scripts/demo-capture.sh ./build/release/skiffllm /path/to/model.gguf
```

Dies zeichnet eine echte Terminal-Sitzung für das README auf; es erfindet
keine Ausgabe.

### Ein kleines Modell beschaffen

```bash
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b
```

Die Dateien werden standardmäßig unter `~/.local/share/skiffllm/models`
gespeichert. Sie können auch mit `--model` auf eine vorhandene `.gguf`-Datei
zeigen.

Die Modelldateien werden unter ihrer eigenen Upstream-Lizenz von Hugging
Face heruntergeladen; SkiffLLM vertreibt sie nicht weiter. Beachten Sie die
Lizenz jedes Modells, bevor Sie es verwenden oder weitergeben. Die Lizenzen im
mitgelieferten Katalog: Qwen2.5/Qwen3 Apache-2.0, SmolLM2 Apache-2.0,
Phi-3.5 MIT und Llama 3.2 **Llama 3.2 Community License** (mit Namens- und
Attributionspflichten).

Vollständige Einrichtung: [docs/de/INSTALL.md](docs/de/INSTALL.md).

---

## Warum SkiffLLM

| | SkiffLLM | Cloud-Assistenten | Ollama |
| --- | --- | --- | --- |
| Cloud erforderlich | ❌ | ✅ | ❌ |
| Cloud-API-Schlüssel | ❌ | ✅ | ❌ |
| Konto / Registrierung | ❌ | ✅ | ❌ |
| Telemetrie | ❌ | unterschiedlich | ❌ |
| Daemon | ❌ | ✅ | ✅ |
| Direkte GGUF-Datei | ✅ | ❌ | teilweise |
| Nur CPU-Maschinen | ✅ | ❌ | ✅ |
| GPU-Offload | ✅ | n/a | ✅ |
| Unix-Pipelines | ✅ | ❌ | ❌ |
| Projekt-/Code-Kontext | ✅ | ❌ | ❌ |
| Lokale OpenAI-kompatible API | ✅ | n/a | ✅ |
| Nativer Android-Client | ✅ | ❌ | Community |
| Nativer iOS-Client | ✅ | ❌ | Community |

Die Laufzeit lädt nie Modelle herunter, sendet keine Daten nach außen und
benötigt kein Konto. Sie bringen die GGUF-Datei mit; SkiffLLM übernimmt die
Inferenz.

### Und warum nicht einfach Ollama?

Kurz gesagt: Nutzen Sie Ollama, wenn Sie einen schnellen, katalogorientierten
Modellserver mit Ein-Zeilen-Downloads wollen; nutzen Sie SkiffLLM, wenn das
Modell wie ein natives Unix-Werkzeug, eine luftspaltige (air-gapped) Komponente
oder eine eingebettete Engine in einem CI-, Desktop- oder Mobil-Workflow
arbeiten soll — ohne Daemon, ein einziges Binary, und Kern-Inferenz, die nie
Verbindung aufnimmt.
Die ehrliche Entscheidungsmatrix, die wirklichen Abwägungen und eine
aufgabenweise Migrationsanleitung finden Sie in
[docs/de/comparison.md](docs/de/comparison.md) und auf Englisch vollständig in
[docs/COMPARISON.md](docs/COMPARISON.md). Für Unternehmensbereitstellung,
Server-Härtung und Supply Chain siehe [docs/ENTERPRISE.md](docs/ENTERPRISE.md)
(englisch).

---

## Highlights

| Funktion | Beschreibung |
| --- | --- |
| Unix-Pipelines | `cat file \| skiffllm "summarize"`, `git diff \| skiffllm "review"` |
| Projektkontext | `--project <dir>` ergänzt echten Dateiindex + begrenzten Quellausschnitt |
| Modellverwaltung | `skiffllm model list / info / install / remove / verify` |
| Git-Integration | `skiffllm git review / explain / commit / log / status` |
| Interaktive Shell | Token-Streaming, Verlauf, Live-Zähler |
| Dateikontext | `--attach`, `/file` und `@file`-Erweiterung in jedem Prompt |
| Gesprächsexport | `--export` und `/export` speichern Sitzungen als Markdown |
| Sitzungen & Speicher | Benannte Sitzungen, `/remember`, `/forget`, dauerhafte Fakten |
| Lokaler API-Server | `--serve` bietet einen OpenAI-kompatiblen Endpunkt |
| Schutz des Servers | Optionales `--api-key` Bearer-Auth für `/v1/*` |
| Echter Benchmark | `--benchmark <runs>` misst echte Prompt- und Generierungsgeschwindigkeit |
| Sampling-Profile | `balanced`, `fast`, `creative`, `code`, `precise` |
| Erweitertes Sampling | temperature, top-p, top-k, min-p, typical-p, Penalties |
| Kontextverwaltung | automatisches Trimmen, reservierter Platz, `/compact` |
| JSON-Modus | maschinenlesbare Ausgabe für Skripte und Tools |
| Diagnose | `--doctor`-Systembericht, `--model-info`, `--tokenize` |
| Native Mobile-Apps | Android (Jetpack Compose) und iOS (SwiftUI) |

---

## Praktische Beispiele

```bash
# Änderungssatz vor dem Push prüfen
git diff | skiffllm "review these changes"

# Die echte Ursache in einem unübersichtlichen Log finden
journalctl -e | skiffllm "find suspicious errors and a likely root cause"

# Eine gerade gelesene Datei zusammenfassen
cat README.md | skiffllm "summarize this"

# Auf ein ganzes Repository zeigen
skiffllm --project . "where is authentication implemented?"

# Maschinenlesbare Ausgabe für eigene Skripte
git diff | skiffllm --json "classify this diff"

# Sichere Code-Review (schlägt Diff vor, ändert nie Dateien)
skiffllm --code --project . "fix the bug in src/server.cpp"
```

## Modellverwaltung

SkiffLLM bleibt zur Laufzeit offline. Das Beschaffen von Modellen ist ein
eigener, expliziter Befehl.

```bash
skiffllm model list
skiffllm model info qwen2.5-0.5b
skiffllm model install qwen2.5-0.5b
skiffllm model verify qwen2.5-0.5b --update
skiffllm model remove qwen2.5-0.5b --force
```

`model install` delegiert an `scripts/model_fetch.py`, das genau ein GGUF per
HTTPS von Hugging Face herunterlädt, den GGUF-Header prüft und eine
SHA-256-Sidecar in Ihr Modellverzeichnis schreibt. Die Kataloggröße ist nur
informativ, weil ein Modellpfleger eine Revision mit anderer Größe neu
hochladen kann. Die Sidecar ist die maßgebliche Integritätsprüfung. Die
Inferenz selbst öffnet niemals eine Verbindung.

## Git-Integration

Lokale, offline Code-Review und Erklärung für den Diff vor Ihnen.

```bash
# Die git-Unterbefehle lesen den Diff selbst; eine Pipe ist nicht nötig.
skiffllm git review
skiffllm git review --cached
skiffllm git explain
skiffllm git commit --cached
skiffllm git log
skiffllm git status
```

`git commit --cached` schlägt aus Ihrem gestageten Diff eine konventionelle
Commit-Message vor; es führt `git commit` nicht für Sie aus.

## Sitzungen & dauerhafter Speicher

```bash
skiffllm --session coding --model qwen2.5-0.5b-instruct-q4_k_m.gguf
skiffllm --session writing --model qwen2.5-0.5b-instruct-q4_k_m.gguf

skiffllm session list
skiffllm session show coding
skiffllm session rename coding writing
skiffllm session remove old-draft
```

Dauerhafte Fakten liegen in `~/.local/share/skiffllm/memories.txt` und verlassen
den Rechner nicht.

```bash
skiffllm --remember "the user prefers concise answers"
skiffllm --forget concise
```

In der interaktiven Shell nutzen Sie `/remember`, `/forget`, `/memories`,
`/clear-memories`, `/compact`, `/regenerate` und `/export`.

---

## Lokaler OpenAI-kompatibler Server

```bash
# nur lokal
skiffllm --model model.gguf --serve --host 127.0.0.1 --port 8080

# nicht-loopback Listener, geschützt durch gemeinsames Token
skiffllm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Endpunkte:

```text
GET  /health                 öffentlicher Health-Check
GET  /version                öffentlicher Versionscheck
GET  /v1/models              Bearer-geschützt, wenn --api-key gesetzt ist
POST /v1/chat/completions    Bearer-geschützt, wenn --api-key gesetzt ist
```

Der Server unterstützt OpenAI-Streaming (`"stream": true`) und beantwortet
schnelle Endpunkte, während eine Generierung läuft. Chat-Generierung ist
serialisiert, weil ein llama.cpp-Kontext nicht thread-sicher ist.

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

Ein Python-Client ohne Abhängigkeiten ist enthalten:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

---

## Mobile Apps

SkiffLLM enthält native Clients für [Android](android/README.md) und
[iOS](ios/README.md). Beide führen unterstützte GGUF-Modelle vollständig auf dem
Gerät aus, streamen Tokens, zeigen eine Live-Kontextleiste und bieten dieselbe
Funktionsfläche wie die Desktop-CLI:

- Pro-Konversation Sampling-Einstellungen und Codemodus
- Multi-Konversation erstellen/öffnen/umbenennen/löschen/sichern/importieren
- Dauerhafte Fakten, Schnellprompts und Share-Sheet-Eingabe
- Stoppsequenzen, Sampling-Profile und Konversationskompression
- Modell-Warmup, echter 3-Runden-Benchmark, Sitzungsstatistik
- Text-/JSON-/XML-Dateianhang (lokal gelesen, begrenzt)
- Markdown-Export und Ein-Tippen-Kopieren
- GGUF-Import mit Header-Prüfung

Die Inferenz selbst öffnet nie eine Verbindung. Es gibt genau zwei
Netzwerkpfade, beide explizit und vom Nutzer gestartet: `model install` lädt
von Hugging Face herunter, und der Unterbefehl `openai` spricht mit einem
HTTP-Server, auf den Sie ihn zeigen. Die Datei muss einen gültigen GGUF-Header
haben und erhält eine SHA-256-Sidecar; die Kataloggröße ist informativ.
Android-Geräte-Backups sind deaktiviert, damit Prompts und Verlauf das Gerät
nie verlassen.

---

## Kommandozeile

```text
Usage: skiffllm [options] [model.gguf]

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

Vollständige Nutzung: [docs/de/usage.md](docs/de/usage.md). Interaktive
Befehle: `/help`, `/warmup`, `/history`, `/stats`, `/compact`,
`/regenerate`, `/tokenize`, `/file`, `/clear-attach`, `/clear`, `/reset`,
`/system`, `/model`, `/profile`, `/stop`, `/temp`, `/top-p`, `/top-k`,
`/min-p`, `/typical`, `/n`, `/ctx`, `/export`, `/save`, `/exit`.

---

## Benchmark-Ehrlichkeit

```bash
skiffllm --model model.gguf --benchmark 3
skiffllm --model model.gguf --benchmark 3 --json
```

Jede Zahl wird auf Ihrer Maschine mit Ihrem Modell und Ihrer Hardware gemessen.
SkiffLLM erfindet keine Benchmark-Ergebnisse. Methode und eine leere Tabelle,
die auf echte Beiträge wartet: [docs/benchmarks.md](docs/benchmarks.md).

## Datenschutz-Nachweis

```bash
skiffllm --doctor --network
```

zeigt die Laufzeitfakten: Die Kern-Generierung tätigt keine ausgehenden
Aufrufe, es gibt keine Telemetrie, keine Cloud-API, und der Verlauf wird lokal
gespeichert. Die einzigen Netzwerkpfade sind explizit und vom Nutzer gestartet:
`model install` (Hugging-Face-Download) und der Unterbefehl `openai` (ein
Server, auf den Sie ihn zeigen). `--serve` öffnet nur einen lokalen Listener
und verbindet sich nie nach außen.

---

## Installation

```bash
bash scripts/install.sh --help
bash scripts/install.sh --prefix "$HOME/.local"
bash scripts/install.sh --prefix /usr/local --backend metal
```

Praktische `Makefile`:

```bash
make release
make tests
make check
make install
make help
```

Fertig gebaute Archive folgen dem Muster
`skiffllm-<version>-<os>-<arch>.tar.gz` (z. B.
`skiffllm-v1.6.0-linux-x86_64.tar.gz`, unter Windows `.zip`) mit
`checksums.txt`, sobald sie veröffentlicht sind. Siehe [docs/de/INSTALL.md](docs/de/INSTALL.md).
Shell-Completions liegen in
[scripts/completions](scripts/completions/).

## Build-Optionen

| Option | Standard | Beschreibung |
| --- | --- | --- |
| `SKIFFLLM_BUILD_TESTS` | `ON` | Testsuite bauen und registrieren |
| `SKIFFLLM_FETCH_LLAMA` | `ON` | Piniertes llama.cpp herunterladen und bauen |
| `SKIFFLLM_LLAMA_SOURCE_DIR` | leer | Vorhandenes llama.cpp-Checkout nutzen |
| `SKIFFLLM_BUILD_SHARED_LLAMA` | `OFF` | llama.cpp als Shared Library bauen |
| `SKIFFLLM_USE_READLINE` | `ON` | GNU Readline aktivieren, wenn vorhanden |
| `SKIFFLLM_LLAMA_BACKEND` | `auto` | `cuda`, `metal`, `vulkan`, `opencl`, `blas`, `cpu` |

Hardware-Beschleunigung ist immer explizit: Backend bei der Konfiguration
wählen und zur Laufzeit mit `--gpu-layers` Layer auslagern.

---

## Anforderungen

- Ein C++17-Compiler (GCC 10+, Clang 12+ oder MSVC 2019+)
- CMake 3.20+
- Optional CMake `FetchContent` für ein piniertes llama.cpp
- Eine GGUF-Modelldatei
- Optional: CUDA/Metal/Vulkan/OpenCL/BLAS für Hardware-Beschleunigung

---

## Projektstruktur

```text
include/skiffllm/              Öffentliche API-Header
src/                          CLI, Kern und lokaler Server
tests/                        Unit-Tests (ohne Modell)
configs/                      Beispielkonfiguration
scripts/                      CI, Release, Modellabruf, API-Client, Completions
android/                      Kotlin/Compose Android-App und llama.cpp JNI
ios/                          SwiftUI iOS-App und llama.cpp Objective-C++-Brücke
docs/                         Setup, Nutzung, Architektur, FAQ, Release-Docs
.github/                      Issue-Vorlagen, PR-Vorlage, CI-Workflow
```

---

## Roadmap

- Embedding- und RAG-Modus
- Paketmanager-Integration (Homebrew, apt, vcpkg)
- GPU-Benchmark-Matrix über Backends
- Grammatik-gebundene Generierung
- Sampling-Diagnose auf Token-Ebene
- Multi-Worker-Generierung über mehrere Modelle

## Bekannte Einschränkungen

SkiffLLM bündelt keine Modelle. Der lokale Server ist standardmäßig auf
`127.0.0.1` öffentlich; verwenden Sie `--api-key`, wenn Sie an ein
Nicht-Loopback-Interface binden. Siehe
[docs/de/LIMITATIONS.md](docs/de/LIMITATIONS.md).

## Mitwirken

Beiträge sind willkommen. Änderungen klein halten, das Offline-Versprechen
wahren, nie Benchmark-Zahlen erfinden und Tests für neues Verhalten ergänzen.
Siehe [CONTRIBUTING.md](CONTRIBUTING.md) und
[docs/de/GOOD_FIRST_ISSUES.md](docs/de/GOOD_FIRST_ISSUES.md). Übersetzungsleitfaden:
[docs/i18n.md](docs/i18n.md).

## Lizenz

MIT — siehe [LICENSE](LICENSE).
