#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-./build/skifflm}"
MODEL="${2:-}"
REPEAT="${3:-3}"

if [[ -z "${MODEL}" ]]; then
    echo "Usage: scripts/bench.sh <binary> <model.gguf> [repeats]"
    exit 2
fi

if [[ ! -x "${BIN}" ]]; then
    echo "Binary not found: ${BIN}"
    exit 2
fi

PROMPT="Write a short poem about the sea."
PHRASE_PATTERN='"tokens_per_second"'

echo "Model: ${MODEL}"
echo "Repeats: ${REPEAT}"
echo

for ((i=1; i<=REPEAT; i+=1)); do
    echo "Run ${i}/${REPEAT}"
    "${BIN}" --model "${MODEL}" --prompt "${PROMPT}" --seed 42 --json --no-save \
        | grep -E "${PHRASE_PATTERN}" || true
done
