# Warum SkiffLLM und nicht einfach Ollama?

Diese Frage stellt jeder neue Nutzer und sie verdient eine direkte, ehrliche
Antwort. SkiffLLM und Ollama lösen überlappende Probleme; die richtige Wahl
hängt von Ihrem Deployment-Modell ab.

Kurz gesagt: **Nutzen Sie Ollama, wenn Sie einen schlanken Modellserver mit
großem Katalog und Ein-Zeilen-Downloads wollen. Nutzen Sie SkiffLLM, wenn das
Modell wie ein natives Unix-Werkzeug, eine luftspaltige (air-gapped) Komponente
oder eine eingebettete Inferenz-Engine in einem CI-/Desktop-/Mobil-Workflow
arbeiten soll — ohne Daemon und ohne Netzwerk zur Laufzeit.**

## Entscheidungsmatrix

| Dimension | SkiffLLM | Ollama |
| --- | --- | --- |
| Netzwerk zur Laufzeit | keine bei der Inferenz | Download/Pull-Dienst standardmäßig |
| Daemon / Hintergrunddienst | keiner (ein Prozess) | ja (`ollama serve`) |
| Modelldatei | eigene GGUF-Datei | verwalteter Katalog, Auto-Pull |
| Pinning / Integrität | SHA-256-Sidecar + `model verify` | Registry-Verweise, weniger explizit |
| Unix-first | native Pipelines, `--project`, `git`-Subcommands | Server + Client, nicht pipe-first |
| Projekt-/Code-Kontext | eingebauter Dateiindex + Quell-Slice | extern über Tools, nicht eingebaut |
| Native Mobile-Apps | Android + iOS in diesem Repo | serverorientiert, Community-Apps |
| Air-gapped / Offline-first | explizites Ziel | konfigurierbar, nicht Standardhaltung |
| OpenAI-kompatible API | `--serve` (Streaming, Bearer-Auth) | ja, primäres Modell |
| Steuerung des Engines | eigenes Build kontrolliert llama.cpp-Backends | gebündelte Laufzeit |
| Datei-/Footprint | ein kleines Binary | Daemon + Laufzeit + Modell-Store |
| Katalogbreite | Sie wählen jedes GGUF | groß, bequem |
| Ökosystem / GUIs (Open WebUI usw.) | Eigenbau | ausgereift |
| Server-Hartgrenzen | eine Generierung, kein internes Rate-Limit | skaliert leichter, größerer Footprint |

Keine Zeile ist für sich ein Fehler. Die Tabelle soll Rätselraten beseitigen.

## Wo SkiffLLM klar die bessere Wahl ist

### 1. Sie wollen ein Unix-Werkzeug, keinen Dienst

Ollama ist server-first: Sie betreiben einen Dienst und sprechen eine HTTP-API.
SkiffLLM ist CLI-first:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
skifflm --project . "where is authentication handled?"
skifflm --code --project . "propose a fix for src/server.cpp"
```

Es gibt keinen Hintergrundprozess, keinen zu verwaltenden Port und keinen
Container mit Residency-Check. Es komponiert mit `jq`, `xargs`, `git` und cron
wie jedes andere Werkzeug.

### 2. Sie sind luftspaltig oder offline-first

Die Laufzeit lädt keine Modelle herunter und enthält keinen Netzcoder. Sie
bringen eine GGUF-Datei mit; SkiffLLM übernimmt die Inferenz. Das ist eine
explizite Haltung statt einer Einstellung, die Sie umschalten müssen. Bei
abgeschirmten oder eingeschränkten Netzen ist das der Unterschied zwischen
„funktioniert standardmäßig" und „funktioniert erst, wenn Sie das Netzwerk
abschalten".

### 3. Sie brauchen Kontrolle über die Inferenz-Engine

SkiffLLM wird gegen das von Ihnen gewählte llama.cpp gebaut. Das Backend wird
bei der Konfiguration über `SKIFFLLM_LLAMA_SOURCE_DIR` und das
Build-Backend-Flag gewählt; `--backend-info` zeigt, was tatsächlich verknüpft
ist. Sie besitzen Compiler, Backend und Binary. Ollama bündelt und verwaltet
seine Laufzeit — bequem, aber weniger transparent.

### 4. Sie wollen Nachweise für die Supply Chain

`skifflm model verify` prüft den GGUF-Magic-Header, die erwartete Dateigröße und
eine SHA-256-Sidecar-Datei. `model_fetch.py --checksum` zeichnet die Sidecar
auf und `--verify` prüft einen vorhandenen Download ohne erneutes Herunterladen.
Das ist die Art von Nachweis, die ein Audit will: welches Modell, welcher Hash,
woher, was wurde gemessen.

### 5. Sie wollen Mobil-Parität aus derselben Engine

Das Repo enthält native Android- und iOS-Clients gegen dieselben Modelldateien
und dasselbe Offline-Versprechen. Das ist keine Ollama-Funktion; Community-
Mobilprojekte existieren, sind aber nicht Teil des Kernprojekts.

### 6. Sie wollen reproduzierbare, ehrliche Benchmarks

`--benchmark` führt echte Generationen auf Ihrer Maschine aus und meldet
gemessene Prompt-Zeit, Generierungszeit und Tokens/s. Die Dokumentation
verlangt vor der Annahme eines Ergebnisses explizit die Befehlsausgabe und den
SHA-256 des Modells. Es gibt keine Marketing-Zahl.

## Wo Ollama die bessere Wahl ist

- **Ein-Befehl-Modellsetup.** `ollama pull llama3` ist einfacher als eine GGUF
  selbst zu beschaffen, herunterzuladen und zu verifizieren.
- **Katalogbreite.** Die offizielle Modellbibliothek ist deutlich größer und
  leichter zu durchsuchen als eine manuelle GGUF-Suche.
- **Server-first-Workloads.** Ist die primäre Schnittstelle HTTP, ist das
  Daemon-Modell in Ordnung, und Open WebUI samt Ökosystem sind ausgereift.
- **Kein Unix-natives Objektmodell nötig.** Wenn die Aufgabe „der Team-Chat
  vor Ort" ist, ist Ollama der reibungsärmere Weg.
- **Mehr-Request-Concurrency.** Ollamas Server ist für viele Clients gebaut.
  SkiffLLM serialisiert die Generierung bewusst hinter einem Mutex, weil ein
  llama.cpp-Kontext nicht thread-safe ist.

## Ehrliche Vorbehalte zu SkiffLLM

- Es bündelt keine Modelle; der Setup-Aufwand ist höher als in einem verwalteten
  Katalog.
- Der `--serve`-Modus ist ein kompakter lokaler Server, kein
  Multi-Thread-Gateway: eine Generierung gleichzeitig, kein internes
  Rate-Limit, optionaler gemeinsamer Bearer-Token.
- Es gibt noch kein großes GUI-Ökosystem.
- Sie müssen prüfen, dass Ihr llama.cpp-Build-Backend Ihre GPU-Hardware
  unterstützt.

## Empfehlung

| Situation | Verwenden |
| --- | --- |
| Shell-first, CI, Git-Review, Offline-Laptops | SkiffLLM |
| Luftsprung / eingeschränktes Netz | SkiffLLM |
| Eingebettet in Desktop-/Mobil-Workflow | SkiffLLM |
| Supply-Chain-Pinning und reproduzierbare Benchmarks | SkiffLLM |
| Schneller Team-Chat mit großem Katalog | Ollama |
| HTTP-first mit Concurrency | Ollama |

Wählen Sie ehrlich, nicht aus Gewohnheit. Wenn Sie vergleichen, starten Sie vom
Deployment-Modell statt von einer Feature-Liste. Produktionsbetrieb: siehe
[docs/ENTERPRISE.md](../ENTERPRISE.md) (englisch), Ollama-Gewohnheiten
übersetzt: [docs/OLLAMA_MIGRATION.md](../OLLAMA_MIGRATION.md).
