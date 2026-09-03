#!/usr/bin/env bash
set -euo pipefail

# Record a short SkiffLLM terminal demo for README/GitHub.
#
# Requires:
#   - a built llm binary
#   - a small GGUF model
#   - asciinema OR a terminal recorder that can export a GIF/SVG
#
# Usage:
#   bash scripts/demo-capture.sh ./build/release/llm /path/model.gguf
#
# The script only records; it never invents output. You must have a real model
# and a real generation to produce an honest demo.

BIN="${1:-./build/release/llm}"
MODEL="${2:-}"

if [[ ! -x "${BIN}" ]]; then
    echo "error: binary not found: ${BIN}" >&2
    echo "usage: bash scripts/demo-capture.sh <binary> <model.gguf>" >&2
    exit 2
fi
if [[ -z "${MODEL}" || ! -f "${MODEL}" ]]; then
    echo "error: a real GGUF model is required: ${MODEL}" >&2
    echo "usage: bash scripts/demo-capture.sh <binary> <model.gguf>" >&2
    exit 2
fi

if ! command -v asciinema >/dev/null 2>&1; then
    echo "asciinema was not found. Install it to record this demo." >&2
    echo "  sudo apt install asciinema   # Debian/Ubuntu" >&2
    echo "  brew install asciinema       # macOS" >&2
    exit 2
fi

echo "Recording demo. Use the shell below to run SkiffLLM, then exit with Ctrl+D."
echo "Recommended commands:"
echo "  ${BIN} --model \"${MODEL}\" --version"
echo "  ${BIN} --model \"${MODEL}\" --doctor --network"
echo "  echo 'Write a one-line joke.' | ${BIN} --model \"${MODEL}\" --no-save"
echo "  echo 'review: add error handling' | ${BIN} --model \"${MODEL}\" --no-save --json"
echo "  git diff | ${BIN} --model \"${MODEL}\" \"review these changes\""
echo
echo "After recording, upload the cast file and convert it to a GIF with:"
echo "  agif delivery  # or asciinema-agg cast.json demo.gif"
echo

asciinema rec llm-demo.cast
