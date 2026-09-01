# Bedienungsanleitung

## Unix-Pipeline-Modus

SkiffLLM erkennt gepipelten stdin automatisch, sodass das Modell Teil eines
Shell-Workflows wird:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
cat README.md | skifflm "summarize this"
skifflm --project . "where is authentication handled?"
```

Ohne Anweisungsargument ist der gepipelte Text selbst der Prompt. Mit einem
Anweisungsargument wird der gepipelte Text zu `<context>`.

## Projektkontext

```bash
skifflm --project . "where is authentication handled?"
```

`--project <dir>` erstellt vor der Generierung einen begrenzten Dateiindex plus
einen Ausschnitt aus Quell-/Konfigurationsdateien. Es überspringt `.git`,
Build-Verzeichnisse, Caches und eingecheckte Abhängigkeiten.

## Sitzungen und Gedächtnis

```bash
skifflm session list
skifflm session show coding
skifflm session rename coding writing
skifflm session remove old-draft

skifflm --remember "the user prefers concise answers"
skifflm --forget concise
```

Interaktive Shell-Befehle: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (eine lange Unterhaltung unter
Beibehaltung von Fakten in eine Aufzählung komprimieren) und `/regenerate`
oder `/retry` (die letzte Benutzernachricht mit den aktuellen
Sampling-Einstellungen erneut ausführen).

## Zusammenfassungs-Kurzbefehl

```bash
skifflm --summarize README.md
skifflm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Modellverwaltung

```bash
skifflm model list
skifflm model info qwen2.5-0.5b
skifflm model install qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b
skifflm model verify qwen2.5-0.5b --update
skifflm model remove qwen2.5-0.5b --force
```

`model verify` prüft den GGUF-Magic-Header, die erwartete Dateigröße und, falls
vorhanden, eine SHA-256-Sidecar-Datei. `--update` zeichnet eine lokale
SHA-256-Sidecar-Datei auf. Downloads über `model_fetch.py` schreiben diese
Sidecar-Datei ebenfalls automatisch und unterstützen `--verify` ohne
erneutes Herunterladen.

Die Inferenz bleibt offline. `model install` führt das explizite
`model_fetch.py`-Hilfsskript über HTTPS aus.

## Einmaliger Lauf und Chat-Vorlagen

```bash
skifflm run "Hello" --ctx 2048 --temp 0.3 --threads 4
skifflm chat-template list
skifflm chat-template detect --model model.gguf
```

## OpenAI-Client

```bash
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
skifflm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Hardwarebeschleunigung

```bash
skifflm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_LLAMA_BACKEND=cuda
./build/skifflm --model model.gguf --gpu-layers -1 --flash-attn
```

Backends werden zur Buildzeit gewählt; `--backend-info` zeigt, was tatsächlich
verknüpft ist.

## Git-Integration

```bash
skifflm git review --cached
skifflm git explain
skifflm git commit --cached
skifflm git log
skifflm git status
```

## Sicherer Codemodus

```bash
skifflm --code --project . "fix the bug in src/server.cpp"
```

`--code` erzeugt einen Unified-Diff-Vorschlag und bearbeitet selbst niemals eine
Datei.

## Interaktiver Modus

```bash
skifflm --model ~/models/model-q4_k_m.gguf
```

Die Oberfläche ist ein Prompt namens `you>`. Geben Sie eine Nachricht ein und
drücken Sie Enter. Der Text wird gestreamt, während er generiert wird.

## Einmalmodus

```bash
skifflm --model model.gguf --prompt "What is recursion?"
```

## Benannte Sitzungen

```bash
skifflm --model model.gguf --session writing
skifflm --model model.gguf --session coding
```

Jede benannte Sitzung erhält eine eigene Verlaufsdatei.

## Systemprompt

```bash
skifflm --model model.gguf --system "You are a patient Python tutor."
```

## Profile

```bash
skifflm --model model.gguf --profile code
```

Verfügbare Profile: `balanced`, `fast`, `creative`, `code`, `precise`.

## Dateikontext

```bash
skifflm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
skifflm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Wiederholen Sie `--attach`, verwenden Sie `@path` im Prompt oder verwalten Sie
Anhänge in der Shell mit `/file` und `/clear-attach`.

## Konversation exportieren

```bash
skifflm --export conversation.md
```

Das Exportieren der geladenen Sitzung benötigt kein Modell und schreibt
Markdown. In der Shell verwenden Sie `/export <path>`.

## Chat-Vorlage und Warmup

```bash
skifflm --model model.gguf --chat-template chatml
skifflm --model model.gguf --warmup
```

`--chat-template` überschreibt den eingebauten Prompt-Formatnamen des Modells.
`--warmup` führt beim Start eine Ein-Token-Generierung aus, um die Latenz der
ersten Antwort zu reduzieren. In der Shell können Sie das Modell mit `/warmup`
erneut aufwärmen.

## Interaktive Zeilenbearbeitung

Wenn GNU Readline verfügbar ist, aktiviert SkiffLLM Pfeiltasten-Navigation,
Verlaufssuche und editierbare Eingabezeilen. Ohne Readline fällt es auf einfaches
`getline` zurück. Die Streaming-Ausgabe zeigt außerdem auf interaktiven
Terminals einen Live-Token-Zähler.

## Stoppsequenzen

```bash
skifflm --model model.gguf --stop "END" --stop "STOP"
```

Die Generierung stoppt bei der ersten konfigurierten Sequenz.

## JSON-Modus

```bash
skifflm --model model.gguf --prompt "Say hello" --json
```

Dies deaktiviert die interaktive Shell und schreibt ein einzelnes
JSON-Objekt nach stdout.

## Piping

```bash
cat prompt.txt | skifflm --model model.gguf
printf 'Explain this command.' | skifflm --model model.gguf --prompt-file /dev/stdin
```

## Ausgabedateien

```bash
skifflm --model model.gguf --prompt-file input.txt --output output.md
```

## Konfigurationsdatei

```bash
skifflm --config ~/.config/skifflm/config
```

Der Standardspeicherort wird verwendet, wenn er vorhanden ist.

## Diagnose

```bash
skifflm --doctor
skifflm --model model.gguf --model-info
skifflm --model model.gguf --smoke
skifflm --model model.gguf --tokenize "hello world"
```

## Lokaler API-Server

```bash
# nur lokal
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# geschützter Nicht-Loopback-Listener
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$SKIFFLLM_SERVER_KEY"
```

Endpunkte:

```text
GET  /health                 öffentlicher Health-Check
GET  /v1/models              Bearer-geschützt, wenn --api-key gesetzt ist
POST /v1/chat/completions    Bearer-geschützt, wenn --api-key gesetzt ist
```

Der Server ist offline, bindet standardmäßig an localhost und unterstützt
OpenAI-Stil-Streaming mit `"stream": true`. Schnelle Endpunkte antworten,
während eine Generierung läuft; Chat-Generierung wird hinter einem Mutex
serialisiert. Wenn `--api-key` konfiguriert ist, erfordern `/v1/*`
`Authorization: Bearer <key>` und geben andernfalls `401` zurück.

Ein schneller Client ist enthalten:

```bash
python3 scripts/api_client.py http://127.0.0.1:8080 "Say hello."
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$SKIFFLLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
skifflm --model model.gguf --benchmark 3
skifflm --model model.gguf --benchmark 3 --json
```

Führt eine echte Generierung aus und meldet gemessene Prompt-Zeit,
Generierungszeit und Tokens pro Sekunde. `--n-predict` steuert die Lauflänge bis
zu 128 Tokens pro Lauf.

## GPU-Offload

Auf einer CUDA-Maschine:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
skifflm --model model.gguf --gpu-layers -1
```

Unter macOS ist das Metal-Backend standardmäßig verfügbar.

## Modellauflistung

```bash
skifflm --model-dir ~/models --list-models
```
