# Security

SkiffLLM is an offline-first local LLM assistant. The core design goal is that
your prompts, conversations, files, and generated text never leave your
machine.

## Threat model

- Trusted local user: the CLI and the Android app run with your local
  privileges.
- Untrusted model files: a GGUF file is arbitrary model data. Only load models
  from sources you trust. Both the Android app and `scripts/model_fetch.py`
  verify the GGUF magic header and record a SHA-256 sidecar, but that is not an
  authenticity guarantee. The catalog byte size is advisory only, so a maintainer
  re-uploading a revision with a new size is not treated as corruption.
- Local server: `--serve` exposes an HTTP API on the bind address. The
  `/v1/*` endpoints are public by default; pass `--api-key` (or set the
  `LLM_API_KEY` / `LLM_SERVER_KEY` environment variable) to require
  `Authorization: Bearer <key>` on `/v1/models` and `/v1/chat/completions`
  while `/health`, `/version`, and `/` stay public. Keep the default
  `127.0.0.1` binding; if you bind to `0.0.0.0`, always set `--api-key` and
  protect the token like a password. The docs use `LLM_SERVER_KEY`; both
  names are accepted.

## Network behavior

- Core inference and the chat shell never open a connection.
- `scripts/model_fetch.py` is an explicit, user-run helper that downloads a
  recommended GGUF over HTTPS from Hugging Face.
- The `openai` subcommand is an explicit client: it sends the prompt you give it
  to an HTTP endpoint you choose (default `http://127.0.0.1:8080`). It is the
  only runtime path that transmits a prompt off the machine, so do not point it
  at a remote server unless that is exactly what you intend.
- `--serve` opens a local listener (bind address) and never dials out.
- The Android app requests `INTERNET` permission only to download models over
  HTTPS from Hugging Face. It does not send prompts, history, analytics, or
  crash reports.

## Reporting issues

Do not open a public issue for a security problem. Contact the maintainer
through the GitHub repository's security advisory flow instead.
