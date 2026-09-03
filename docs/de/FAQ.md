# FAQ

## Lädt SkiffLLM meine Unterhaltungen hoch?

Nein. Die Desktop-Laufzeitumgebung enthält keinen Netzcoder. Prompts, Verlauf,
Einstellungen und generierter Text bleiben auf der Maschine. Die Android-App
sendet keine Prompts irgendwohin.

## Lädt SkiffLLM Modelle herunter?

Nicht automatisch. Die Desktop-Laufzeitumgebung benötigt eine GGUF-Datei. Zwei
optionale Hilfsskripte sind verfügbar:

- Desktop: `python3 scripts/model_fetch.py --model qwen2.5-0.5b`
- Android: `Settings` → `Models` → `Download`

Beide verwenden HTTPS von Hugging Face und sind explizite Benutzeraktionen.

## Welches Modell sollte ich verwenden?

Beginnen Sie mit einem kleinen Instruct-GGUF im Q4_K_M-Format:

- Qwen2.5-0.5B-Instruct
- Qwen3-0.6B-Instruct
- Llama-3.2-1B-Instruct

Auf einem Telefon mit weniger als 4 GB RAM verwenden Sie die 0.5B- oder
0.6B-Modelle.

## Warum ist meine erste Antwort langsam?

Der erste Durchlauf nach dem Laden des Modells umfasst Prompt-Verarbeitung und
Page-Fault-Warmup. Verwenden Sie auf dem Desktop `--warmup` oder starten Sie die
Android-App einmal vor einem echten Gespräch. Die Android-App wärmt das Modell
nach dem Laden automatisch auf.

## Wo werden Sitzungen gespeichert?

Desktop: das Sitzungsverzeichnis aus `--session`/`--history` (standardmäßig unter
`~/.local/share/llm`). Android: `conversation.json` im App-internen Speicher;
das Löschen der App-Daten entfernt sie.

## Wie stelle ich die lokale API bereit?

```bash
# nur lokal
llm --model model.gguf --serve --host 127.0.0.1 --port 8080

# von einer anderen Maschine erreichbar, durch gemeinsamen Token geschützt
llm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "local-token"
```

Mit `--api-key` erfordern `/v1/models` und `/v1/chat/completions`
`Authorization: Bearer <key>` und geben andernfalls `401` zurück. `/health`,
`/version` und `/` bleiben öffentlich. Behalten Sie die Standardbindung
`127.0.0.1` wann immer möglich bei; Nicht-Loopback-Listener sollten immer
`--api-key` setzen.

## Warum unterscheidet sich `--benchmark` von Herstellerzahlen?

Jeder Benchmark in SkiffLLM wird auf Ihrer Maschine gemessen, mit der von Ihnen
bereitgestellten Modelldatei und Hardware. Die Zahlen hängen von CPU/GPU,
Quantisierung, Kontextgröße, Threads und Systemlast ab. Es gibt keine erfundenen
oder Marketing-Zahlen.

## Läuft die Android-App ohne Netzwerkverbindung?

Ja, nachdem ein Modell geladen wurde. Das Laden eines gespeicherten Modells,
Chatten, Exportieren und Löschen funktionieren alles offline. Das Netzwerk wird
nur verwendet, wenn Sie ein neues Modell von Hugging Face herunterladen möchten.

## Ist Telemetrie aktiviert?

Nein. Es gibt keine Analytik, keine Crash-Berichterstattung und kein
Nutzungs-Tracking in der Desktop-Laufzeitumgebung oder der Android-App.
