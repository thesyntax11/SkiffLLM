# Moving from Ollama to SkiffLLM

This is a task-for-task translation of Ollama habits into SkiffLLM. It does not
claim SkiffLLM is a drop-in replacement; it maps the actions you already know to
their closest real equivalent.

## Habit mapping

| Ollama | SkiffLLM |
| --- | --- |
| `ollama pull <model>` | `python3 scripts/model_fetch.py --model <id>` (or copy a GGUF you already have) |
| `ollama run <model>` | `skifflm --model <file>` (or `skifflm run "<prompt>"`) |
| `ollama serve` | `skifflm --serve --host 127.0.0.1 --port 8080` |
| `ollama list` | `skifflm model list` / `skifflm --list-models` |
| `ollama ps` | no resident daemon — there is no persistent process |
| `ollama show` | `skifflm --model <file> --model-info` |
| `ollama rm` | `skifflm model remove <id> --force` |
| `ollama cp` | not applicable; sessions are copied with the session files |
| OpenAI-compatible HTTP | same OpenAI-style API under `--serve` (`/v1/chat/completions`, `stream`) |

## Key differences to plan for

- **You bring the model.** `skifflm` does not download at runtime. Use
  `scripts/model_fetch.py` or place a GGUF in the model directory.
- **No daemon.** Every invocation is a process. There is no `serve` running
  unless you start one. This is a feature for CLI workflows, not a limitation.
- **GGUF path is first-class.** `--model` accepts any existing `.gguf`; the
  model directory is a fallback, not a controller-managed store.
- **One generation at a time over the local API.** A llama.cpp context is not
  thread-safe, so `/v1/chat/completions` is serialized behind a mutex. Scale with
  multiple processes, not threads, if you need concurrency.

## Small migration checklist

1. Get a GGUF: `python3 scripts/model_fetch.py --model qwen2.5-0.5b`.
2. Record its hash: `python3 scripts/model_fetch.py --verify` or
   `skifflm model verify <id>`.
3. Run once: `skifflm --model <file> --model-info`.
4. Move your pipeline: `git diff | skifflm "review"` instead of
   `curl localhost:11434/api/chat`.
5. For a service, start the local server only where you need it:
   `skifflm --serve --host 127.0.0.1 --port 8080 --api-key "$KEY"`.
6. Confirm what you adopted with `skifflm --doctor` and `--backend-info`.

## When to stay on Ollama

If your team already relies on the Ollama catalogue, Open WebUI, and a
server-first multi-client deployment, do not switch for the sake of it. The
honest comparison and the recommendation table are in
[docs/COMPARISON.md](COMPARISON.md).
