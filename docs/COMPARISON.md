# Why SkiffLLM and not just Ollama?

This is the question every new user asks, and it deserves a direct, honest
answer. SkiffLLM and Ollama solve overlapping problems, so the right choice
depends on your deployment model.

The short version: **Use Ollama when you want a streamlined model server with a
large catalogue and one-line downloads. Use SkiffLLM when you want the model to
behave like a native Unix tool, an air-gapped component, or an embedded
inference engine in a CI/desktop/mobile workflow — with no daemon and core
inference that never connects.**

## Decision matrix

| Dimension | SkiffLLM | Ollama |
| --- | --- | --- |
| Runtime network use | none at inference | download/pull service by default |
| Daemon / background service | none (single process) | yes (`ollama serve`) |
| Model file | bring your own GGUF | managed catalogue, auto-pull |
| Model pinning / integrity | SHA-256 sidecar + `model verify` | registry references, less explicit |
| Unix-first usage | native pipelines, `--project`, `git` subcommands | server + client CLI, not pipe-first |
| Project/code context | built-in file index + source slice | via tools externally, not built-in |
| Native mobile clients | Android + iOS in this repo | server-oriented, community backends |
| Air-gapped / offline-first | explicit goal | can be configured, not the default posture |
| OpenAI-compatible API | `--serve` (streaming, bearer auth) | yes, primary model |
| Controller/engine split | your build controls llama.cpp backends | bundled runtime |
| File/server footprint | one small binary | daemon + runtime + model store |
| Model catalogue breadth | you choose any GGUF | large, convenient |
| Ecosystem / GUI (Open WebUI etc.) | DIY | mature |
| Server hard limits | single generation, no internal rate limit | scales more readily, larger footprint |

Neither row is a defect by itself. The table is meant to remove the guesswork.

## Where SkiffLLM is clearly the better fit

### 1. You want a Unix tool, not a service

Ollama is server-first: you run a service and talk to an HTTP API. SkiffLLM is
CLI-first:

```bash
git diff | skifflm "review these changes"
cat error.log | skifflm "find the root cause"
skifflm --project . "where is authentication handled?"
skifflm --code --project . "propose a fix for src/server.cpp"
```

There is no background process, no port to manage, no container carrying a
residency check. It composes with `jq`, `xargs`, `git`, and cron the way any
other tool does.

### 2. You are air-gapped or offline-first

Core inference never connects; you bring a GGUF file and SkiffLLM runs it
locally. There are exactly two opt-in network paths: `model install` (Hugging
Face download) and the `openai` client (a server you point it at). For
classified, restricted, or isolated
networks this is the difference between "works by default" and "works only after
you disable networking."

### 3. You need control over the inference engine

SkiffLLM builds against the llama.cpp you choose. The backend is decided when
you configure `SKIFFLLM_LLAMA_SOURCE_DIR` and the build backend flag, and
`--backend-info` tells you what is actually linked. You own the compiler, the
backend, and the binary. Ollama bundles and manages its runtime for you, which is
convenient but less transparent.

### 4. You want supply-chain evidence

`skifflm model verify` checks the GGUF magic header and verifies the SHA-256
sidecar. The catalog size is advisory so a newer upstream revision is not
rejected by a stale byte count. `model_fetch.py --checksum` records the sidecar and
`--verify` checks an existing download without re-downloading. This is the kind
of evidence an audit wants: what model, what hash, where it came from, what was
measured.

### 5. You want mobile parity from the same engine

The repository ships native Android and iOS clients against the same model files
and the same offline promise. This is not an Ollama feature; community mobile
projects exist but are not part of the core project.

### 6. You want reproducible, honest benchmarks

`--benchmark` runs real generations on your machine and reports measured prompt
time, generation time, and tokens/s. The docs explicitly ask for the command
output and the model SHA-256 before accepting a result. There is no marketing
number.

## Where Ollama is the better fit

- **One-command model setup.** `ollama pull llama3` is simpler than sourcing,
  downloading, and verifying a GGUF yourself.
- **Catalogue breadth.** The official model library is much larger and easier to
  browse than a manual GGUF search.
- **Server-first workloads.** If the primary interface is HTTP, the daemon model
  is fine, and Open WebUI and the surrounding ecosystem are mature.
- **You do not need a Unix-native object model.** If the task is "give the team a
  local chat box", Ollama is the lower-friction path.
- **Multi-request concurrency.** Ollama's server is built to serve many clients.
  SkiffLLM intentionally serializes generation behind one mutex because a
  llama.cpp context is not thread-safe.

## Honest caveats about SkiffLLM

- It does not bundle models; setup cost is higher than a managed catalogue.
- The `--serve` mode is a compact local server, not a multi-threaded gateway:
  one generation at a time, no internal rate limiting, optional shared bearer
  token.
- There is no large ecosystem of GUIs yet.
- You must verify your llama.cpp build backend supports your GPU hardware.

## Recommendation

| Situation | Use |
| --- | --- |
| Shell-first, CI, git review, offline laptops | SkiffLLM |
| Air-gapped / restricted network | SkiffLLM |
| Embedded in desktop/mobile workflow | SkiffLLM |
| Supply-chain pinning and reproducible benchmarks | SkiffLLM |
| Fast team chat box with a big catalogue | Ollama |
| HTTP-first service with concurrency | Ollama |

Choose honestly, not by habit. If you are comparing, start from your deployment
model rather than from a feature list. See [docs/ENTERPRISE.md](ENTERPRISE.md)
for production deployment and [docs/OLLAMA_MIGRATION.md](OLLAMA_MIGRATION.md)
for a translation of Ollama habits to SkiffLLM.
