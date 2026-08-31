#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
SOURCE_DIR="${1:-}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

mkdir -p "${BUILD_DIR}"
cd "${PROJECT_DIR}"

if [[ -n "${SOURCE_DIR}" ]]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DSKIFFLLM_LLAMA_SOURCE_DIR="${SOURCE_DIR}" \
        -DSKIFFLLM_FETCH_LLAMA=OFF
else
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
fi

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j

echo
echo "SkiffLLM was built successfully."
echo "Run it with: ${BUILD_DIR}/skifflm --help"
echo "Run the tests with: ctest --test-dir ${BUILD_DIR} --output-on-failure"
