#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
SOURCE_DIR="${1:-}"

mkdir -p "${BUILD_DIR}"
cd "${PROJECT_DIR}"

if [[ -n "${SOURCE_DIR}" ]]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIFFLLM_LLAMA_SOURCE_DIR="${SOURCE_DIR}"
else
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release
fi

cmake --build "${BUILD_DIR}" --config Release -j

echo
echo "SkiffLLM was built successfully."
echo "Run it with: ${BUILD_DIR}/skifflm --help"
