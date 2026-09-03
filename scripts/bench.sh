#!/usr/bin/env bash
set -euo pipefail

# Run a reproducible SkiffLLM benchmark and print a plain-text table row.
#
# Usage:
#   bash scripts/bench.sh <binary> <model.gguf> [repeats]
#
# The output is intended to be copied into docs/benchmarks.md once someone
# verifies it on a real machine. Numbers are never invented here.

BIN="${1:-./build/release/llm}"
MODEL="${2:-}"
REPEAT="${3:-3}"

if [[ -z "${MODEL}" ]]; then
    echo "Usage: scripts/bench.sh <binary> <model.gguf> [repeats]" >&2
    exit 2
fi
if [[ ! -x "${BIN}" ]]; then
    echo "Binary not found: ${BIN}" >&2
    exit 2
fi
if [[ "${REPEAT}" -lt 1 ]]; then
    echo "Error: repeats must be at least 1" >&2
    exit 2
fi

echo "=== Environment ==="
"${BIN}" --version
"${BIN}" --backend-info

echo "=== SHA-256 (model file) ==="
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${MODEL}"
else
    shasum -a 256 "${MODEL}"
fi

echo "=== Benchmark ==="
echo "Model: ${MODEL}"
echo "Repeats: ${REPEAT}"
echo

"${BIN}" --model "${MODEL}" --benchmark "${REPEAT}" --seed 42 --json --no-save
