# Bedienungsanleitung

## Unix-Pipeline-Modus

SkiffLLM erkennt gepipelten stdin automatisch, sodass das Modell Teil eines
Shell-Workflows wird:

```bash
git diff | llm "review these changes"
cat error.log | llm "find the root cause"
cat README.md | llm "summarize this"
llm --project . "where is authentication handled?"
```

Ohne Anweisungsargument ist der gepipelte Text selbst der Prompt. Mit einem
Anweisungsargument wird der gepipelte Text zu `<context>`.

## Projektkontext

```bash
llm --project . "where is authentication handled?"
```

`--project <dir>` erstellt vor der Generierung einen begrenzten Dateiindex plus
einen Ausschnitt aus Quell-/Konfigurationsdateien. Es überspringt `.git`,
Build-Verzeichnisse, Caches und eingecheckte Abhängigkeiten.

## Sitzungen und Gedächtnis

```bash
llm session list
llm session show coding
llm session rename coding writing
llm session remove old-draft

llm --remember "the user prefers concise answers"
llm --forget concise
```

Interaktive Shell-Befehle: `/remember <fact>`, `/forget <text>`,
`/memories`, `/clear-memories`, `/compact` (eine lange Unterhaltung unter
Beibehaltung von Fakten in eine Aufzählung komprimieren) und `/regenerate`
oder `/retry` (die letzte Benutzernachricht mit den aktuellen
Sampling-Einstellungen erneut ausführen).

## Zusammenfassungs-Kurzbefehl

```bash
llm --summarize README.md
llm --summarize error.log --model qwen2.5-0.5b.gguf
```

## Modellverwaltung

```bash
llm model list
llm model info qwen2.5-0.5b
llm model install qwen2.5-0.5b
llm model verify qwen2.5-0.5b
llm model verify qwen2.5-0.5b --update
llm model remove qwen2.5-0.5b --force
```

`model verify` prüft den GGUF-Magic-Header und, falls vorhanden, die
SHA-256-Sidecar-Datei. Die Kataloggröße ist nur informativ; eine neuere
Upstream-Revision wird nicht allein wegen einer geänderten Bytezahl abgelehnt.
`--update` zeichnet eine lokale
SHA-256-Sidecar-Datei auf. Downloads über `model_fetch.py` schreiben diese
Sidecar-Datei ebenfalls automatisch und unterstützen `--verify` ohne
erneutes Herunterladen.

Die Inferenz bleibt offline. `model install` führt das explizite
`model_fetch.py`-Hilfsskript über HTTPS aus.

## Einmaliger Lauf und Chat-Vorlagen

```bash
llm run "Hello" --ctx 2048 --temp 0.3 --threads 4
llm chat-template list
llm chat-template detect --model model.gguf
```

## OpenAI-Client

```bash
llm openai "Merhaba" --base-url http://127.0.0.1:8080
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --stream
llm openai "Merhaba" --base-url http://127.0.0.1:8080 --no-json
```

## Hardwarebeschleunigung

```bash
llm --backend-info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLM_LLAMA_BACKEND=cuda
./build/llm --model model.gguf --gpu-layers -1 --flash-attn
```

Backends werden zur Buildzeit gewählt; `--backend-info` zeigt, was tatsächlich
verknüpft ist.

## Git-Integration

```bash
llm git review --cached
llm git explain
llm git commit --cached
llm git log
llm git status
```

## Sicherer Codemodus

```bash
llm --code --project . "fix the bug in src/server.cpp"
```

`--code` erzeugt einen Unified-Diff-Vorschlag und bearbeitet selbst niemals eine
Datei.

## Interaktiver Modus

```bash
llm --model ~/models/model-q4_k_m.gguf
```

Die Oberfläche ist ein Prompt namens `you>`. Geben Sie eine Nachricht ein und
drücken Sie Enter. Der Text wird gestreamt, während er generiert wird.

## Einmalmodus

```bash
llm --model model.gguf --prompt "What is recursion?"
```

## Benannte Sitzungen

```bash
llm --model model.gguf --session writing
llm --model model.gguf --session coding
```

Jede benannte Sitzung erhält eine eigene Verlaufsdatei.

## Systemprompt

```bash
llm --model model.gguf --system "You are a patient Python tutor."
```

## Profile

```bash
llm --model model.gguf --profile code
```

Verfügbare Profile: `balanced`, `fast`, `creative`, `code`, `precise`.

## Dateikontext

```bash
llm --model model.gguf --attach notes.txt --prompt "Summarize these notes."
llm --model model.gguf --prompt "Read @notes.txt and list the tasks."
```

Wiederholen Sie `--attach`, verwenden Sie `@path` im Prompt oder verwalten Sie
Anhänge in der Shell mit `/file` und `/clear-attach`.

## Konversation exportieren

```bash
llm --export conversation.md
```

Das Exportieren der geladenen Sitzung benötigt kein Modell und schreibt
Markdown. In der Shell verwenden Sie `/export <path>`.

## Chat-Vorlage und Warmup

```bash
llm --model model.gguf --chat-template chatml
llm --model model.gguf --warmup
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
llm --model model.gguf --stop "END" --stop "STOP"
```

Die Generierung stoppt bei der ersten konfigurierten Sequenz.

## JSON-Modus

```bash
llm --model model.gguf --prompt "Say hello" --json
```

Dies deaktiviert die interaktive Shell und schreibt ein einzelnes
JSON-Objekt nach stdout.

## Piping

```bash
cat prompt.txt | llm --model model.gguf
printf 'Explain this command.' | llm --model model.gguf --prompt-file /dev/stdin
```

## Ausgabedateien

```bash
llm --model model.gguf --prompt-file input.txt --output output.md
```

## Konfigurationsdatei

```bash
llm --config ~/.config/llm/config
```

Der Standardspeicherort wird verwendet, wenn er vorhanden ist.

## Diagnose

```bash
llm --doctor
llm --model model.gguf --model-info
llm --model model.gguf --smoke
llm --model model.gguf --tokenize "hello world"
```

## Lokaler API-Server

```bash
# nur lokal
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# geschützter Nicht-Loopback-Listener
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "$LLM_SERVER_KEY"
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
python3 scripts/api_client.py http://127.0.0.1:8080 --api-key "$LLM_SERVER_KEY" "Say hello."
```

## Benchmark

```bash
llm --model model.gguf --benchmark 3
llm --model model.gguf --benchmark 3 --json
```

Führt eine echte Generierung aus und meldet gemessene Prompt-Zeit,
Generierungszeit und Tokens pro Sekunde. `--n-predict` steuert die Lauflänge bis
zu 128 Tokens pro Lauf.

## GPU-Offload

Auf einer CUDA-Maschine:

```bash
cmake -S . -B build -DGGML_CUDA=ON
cmake --build build -j
llm --model model.gguf --gpu-layers -1
```

Unter macOS ist das Metal-Backend standardmäßig verfügbar.

## Modellauflistung

```bash
llm --model-dir ~/models --list-models
```
