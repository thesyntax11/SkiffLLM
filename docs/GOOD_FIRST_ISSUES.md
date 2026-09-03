# Good First Issues

These are small, well-scoped tasks that are a good way to learn the codebase.
They should always be honest: no invented benchmarks, no fake features, and no
changes that break the offline-first runtime.

## Labeled areas

- **good first issue** — small, self-contained tasks.
- **help wanted** — tasks that may need discussion.
- **documentation** — docs, examples, and README help.
- **enhancement** — new features behind a clear design.
- **performance** — speed or memory work.
- **android** — Kotlin/Compose, JNI, or the native Android build.

## Starter ideas

1. Add a completion entry for the new `model` and `git` subcommands in
   `scripts/completions/`.
2. Add one more honest benchmark run to `docs/benchmarks.md` with the exact
   command, model hash, and machine details.
3. Add a test that checks `skiffllm model list` output contains the expected
   catalog ids.
4. Improve error handling in `src/server.cpp` when `Content-Length` is missing.
5. Add a cross-platform `scripts/ci.ps1` that mirrors `scripts/ci-local.sh`.
6. Create a reproducible `docs/demo.md` transcript and link it from the README.

## Before you start

- Read `CONTRIBUTING.md` and `SECURITY.md`.
- Keep the runtime offline.
- Never copy benchmark numbers from another machine.

## Looking for more?

Raise an issue or open a draft PR with a proposed task; maintainers will label
it and help scope the work.
