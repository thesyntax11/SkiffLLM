# FAQ

## Does SkiffLLM upload my conversations?

No. Core inference never sends prompts anywhere, and the Android app sends no
prompts anywhere either. The desktop runtime has two opt-in network paths that
you invoke deliberately: `model install` (a Hugging Face download, no prompt
data) and the `openai` client (which sends the prompt to a server you choose;
keep it on localhost unless you intend otherwise). `--serve` only listens.

## Does SkiffLLM download models?

Not automatically. The desktop runtime requires a GGUF file. Two optional
helpers are available:

- Desktop: `python3 scripts/model_fetch.py --model qwen2.5-0.5b`
- Android: `Settings` → `Models` → `Download`

Both use HTTPS from Hugging Face and are explicit user actions.

## What model should I use?

Start with a small instruct GGUF in Q4_K_M format:

- Qwen2.5-0.5B-Instruct
- Qwen3-0.6B-Instruct
- Llama-3.2-1B-Instruct

On a phone with less than 4 GB RAM, use the 0.5B or 0.6B models.

## Why is my first answer slow?

The first pass after a model load includes prompt processing and page-fault
warm-up. Use `--warmup` on desktop or start the Android app once before a real
conversation. The Android app warms the model automatically after loading.

## Where are sessions stored?

Desktop: the session directory from `--session`/`--history` (defaults under
`~/.local/share/skifflm`). Android: app-internal `conversation.json`; clearing
the app data removes it.

## How do I expose the local API?

```bash
# local only
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# reachable from another machine, protected by a shared token
skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 --api-key "local-token"
```

With `--api-key` set, `/v1/models` and `/v1/chat/completions` require
`Authorization: Bearer <key>` and otherwise return `401`. `/health`,
`/version`, and `/` stay public. Keep the default `127.0.0.1` binding whenever
possible; non-loopback listeners should always set `--api-key`.

## Why does `--benchmark` differ from vendor numbers?

Every benchmark in SkiffLLM is measured on your machine, with the model file and
hardware you provide. Numbers depend on CPU/GPU, quantization, context size,
threads, and system load. There are no fake or marketing numbers.

## Does the Android app run without a network connection?

Yes, after a model is loaded. Loading a saved model, chatting, exporting, and
clearing all work offline. Network is used only if you choose to download a new
model from Hugging Face.

## Is telemetry enabled?

No. There is no analytics, crash reporting, or usage tracking in the desktop
runtime or the Android app.
