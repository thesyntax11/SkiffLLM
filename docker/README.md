# SkiffLLM in Docker

A multi-stage image that builds the desktop binary and its llama.cpp dependency.
The image never contains a model.

## Build and run

```bash
docker build -t skifflm:local .
docker run --rm -p 127.0.0.1:8080:8080 \
  -v "$PWD/models:/models:ro" \
  skifflm:local \
  --model /models/model-q4_k_m.gguf \
  --serve --host 0.0.0.0 --port 8080 \
  --api-key "$SKIFFLLM_SERVER_KEY"
```

The `--host 0.0.0.0` is needed inside the container; the Docker port is still
bound to `127.0.0.1` on the host, so the listener is not exposed to the network.

## Compose

```bash
cp .env.example .env          # then set SKIFFLLM_SERVER_KEY
mkdir -p models               # put model-q4_k_m.gguf here
docker compose up -d --build
docker compose logs -f skifflm
```

## Security notes

- Models are mounted read-only (`:ro`) so the image cannot modify them.
- The default compose file binds only to the host loopback.
- Use `--api-key` (shared bearer token) whenever you change the host binding.
- The image runs as a non-root user.

See [docs/ENTERPRISE.md](../docs/ENTERPRISE.md) for the full server-hardening
runbook.
