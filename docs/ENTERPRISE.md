# Enterprise deployment and operations

This guide treats SkiffLLM as an inference component that must be deployed,
hardened, observed, and audited. The operating posture is explicit and
non-negotiable: **no cloud dependency, no telemetry, no runtime network calls
from core inference, and a model you bring.** The only runtime path that sends
a prompt off the machine is the explicit `openai` subcommand, aimed at an
endpoint you choose; keep it pointed at localhost unless you intend otherwise.

## 1. Posture

- Binary: one small, statically linked executable talking to llama.cpp.
- Network: no network code at inference; the only network features are the
  optional server listener `--serve`, the explicit `model_fetch.py` / Android
  download helpers, and the `openai` client you point at a server.
- Data: prompts, history, settings, sessions, and generated text stay on the
  machine (or inside the app sandbox on mobile).
- Models: a GGUF file you supply; never bundled, never silently fetched.

## 2. Reference topologies

### 2.1 Developer laptop / CI runner (CLI-first)

```bash
skifflm --model ./model-q4_k_m.gguf --project . "review the change"
git diff | skifflm "does this preserve the offline guarantee?"
```

No daemon, no port, no persistent service. The process exits when the task ends.

### 2.2 Air-gapped host

- Build once on an internet-connected machine with the llama.cpp backend you need.
- Copy the binary plus the signed/verified GGUF to the isolated host.
- Record the SHA-256 sidecar during build or import.

### 2.3 Dedicated inference host (local server)

```bash
# loopback only — the safe default for a workstation service
skifflm --model model.gguf --serve --host 127.0.0.1 --port 8080

# protected listener behind a reverse proxy; never expose without a token
SKIFFLLM_SERVER_KEY="$(openssl rand -hex 24)" \
  skifflm --model model.gguf --serve --host 0.0.0.0 --port 8080 \
  --api-key "$SKIFFLLM_SERVER_KEY"
```

`/health`, `/version`, and `/` stay public; `/v1/models` and
`/v1/chat/completions` require `Authorization: Bearer <key>` when
`--api-key` is set.

### 2.4 Mobile (managed device)

Android and iOS clients use the same offline model files. On managed devices,
keep model downloads in the app (explicit user action) and treat the app storage
as the conversation boundary.

### 2.5 Container (optional)

A multi-stage Dockerfile is provided for the server topology. The image builds
the binary and runs as a non-root user. Models are mounted at runtime; they are
never baked into the image. See `docker/`.

## 3. Server hardening

These are the current real limits; plan around them rather than assuming more.

- **Bind loopback by default.** `127.0.0.1` is the only safe public-to-private
  default.
- **Use a bearer token for any non-loopback listener.** SkiffLLM uses shared
  `--api-key` auth; it is not a per-user identity system.
- **Put a TLS-terminating reverse proxy in front** of a non-loopback listener.
  SkiffLLM has no TLS layer.
- **Rate limit at the proxy.** SkiffLLM has no internal rate limiter.
- **One generation at a time.** A llama.cpp context is not thread-safe, so chat
  generation is serialized behind a mutex. Fast endpoints continue to answer.
- **Do not expose `/v1/*` without a token.** Without `--api-key` they are public
  and it is only safe while listening on `127.0.0.1`.

## 4. Model supply chain

```bash
# inventory and verify in one pass
python3 scripts/model_fetch.py --list
python3 scripts/model_fetch.py --model qwen2.5-0.5b --checksum
python3 scripts/model_fetch.py --model qwen2.5-0.5b --verify
skifflm model verify qwen2.5-0.5b
```

The sidecar is `<model>.sha256`. Record it at import time and re-verify before
promotion. This is the evidence path for a model provenance question: file,
hash, size, backend.

## 5. Audit and data handling

- **No telemetry.** There is no analytics, crash reporting, or usage tracking in
  the desktop runtime or the Android app.
- **Session files** live under `--session`/`--history` (default
  `~/.local/share/skifflm`). Treat them like any prompt data.
- **Exports** are Markdown written by `--export` / `/export`; only export what
  you intend to store.
- **Logs.** The compact server has no request log; keep proxy/access logs at the
  reverse proxy if you need an audit trail. This is a known limitation.
- **Remove or encrypt** session and memory files when the workflow requires it;
  there is no at-rest encryption.

## 6. CI usage

```bash
# non-interactive, machine-readable
skifflm --json --model model.gguf --project . "summarize the diff"

# code review that proposes but never edits
skifflm --code --project . "suggest a fix for src/server.cpp"
```

Because generation is serialized and context is bounded, keep CI jobs on small
models and set a generation cap (`--n-predict`). Never run a background server
in a job that does not need one.

## 7. Sizing

- **CPU-only small model:** 0.5B-1.5B Q4_K_M is a fast start.
- **Context:** configured context bounds the prompt; use `--auto-trim` for long
  conversations.
- **GPU:** build with the backend your hardware supports
  (`CUDA`, `Vulkan`, `Metal`, `ROCm`, `SYCL`); verify with `--backend-info`.
- **Threads:** default thread count is usually fine; tune `--threads` on large
  hosts.

## 8. Runbook

1. `skifflm --doctor` — system report.
2. `skifflm --backend-info` — confirm the linked backend.
3. `skifflm --model <model> --model-info` — model metadata.
4. `scripts/enterprise-check.sh` — repository preflight checks.
5. If generation stops at the context limit, raise `--ctx` or enable
   `--auto-trim`.

## 9. What it is not

SkiffLLM's server is a compact local tool, not a multi-tenant gateway. It does
not provide per-user auth, TLS, internal rate limiting, or request logging, and
it does not bundle models. If you need those as first-class features, the
honest move is to stand up a production gateway (or use a service-oriented
runtime) and keep SkiffLLM as the offline/CLI/embedded inference engine.

See [docs/COMPARISON.md](COMPARISON.md) to choose the right tool for the job and
[docs/LIMITATIONS.md](LIMITATIONS.md) for the full constraint list.
