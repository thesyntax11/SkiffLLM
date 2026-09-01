# FAQ

## Does SkiffLLM upload my conversations?

No. The desktop runtime has no network code. Prompts, history, settings, and
generated text stay on the machine. The Android app sends no prompts anywhere.

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
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080
```

Keep it on `127.0.0.1`. The server has no authentication. If you bind to
`0.0.0.0`, protect that interface yourself.

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
