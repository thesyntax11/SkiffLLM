# Security

SkiffLLM is an offline-first local LLM assistant. The core design goal is that
your prompts, conversations, files, and generated text never leave your
machine.

## Threat model

- Trusted local user: the CLI and the Android app run with your local
  privileges.
- Untrusted model files: a GGUF file is arbitrary model data. Only load models
  from sources you trust. Both the Android app and `scripts/model_fetch.py`
  verify the GGUF header and download size, but that is not an authenticity
  guarantee.
- Local server: `--serve` exposes an HTTP API on the bind address. The
  `/v1/*` endpoints are public by default; pass `--api-key` (or set
  `SKIFFLLM_API_KEY`) to require `Authorization: Bearer <key>` on `/v1/models`
  and `/v1/chat/completions` while `/health`, `/version`, and `/` stay public.
  Keep the default `127.0.0.1` binding; if you bind to `0.0.0.0`, always set
  `--api-key` and protect the token like a password.

## Network behavior

- The desktop runtime does not make network requests.
- `scripts/model_fetch.py` is an explicit, user-run helper that downloads a
  recommended GGUF over HTTPS from Hugging Face.
- The Android app requests `INTERNET` permission only to download models over
  HTTPS from Hugging Face. It does not send prompts, history, analytics, or
  crash reports.

## Reporting issues

Do not open a public issue for a security problem. Contact the maintainer
through the GitHub repository's security advisory flow instead.
