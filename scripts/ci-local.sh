#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_DIR}/build}"
SOURCE_DIR="${SKIFFLLM_LLAMA_SOURCE_DIR:-}"

cd "${PROJECT_DIR}"

if [[ -n "${SOURCE_DIR}" ]]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIFFLLM_LLAMA_SOURCE_DIR="${SOURCE_DIR}" \
        -DSKIFFLLM_FETCH_LLAMA=OFF \
        -DSKIFFLLM_BUILD_TESTS=ON
else
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIFFLLM_BUILD_TESTS=ON
fi

cmake --build "${BUILD_DIR}" --config Release -j

ctest --test-dir "${BUILD_DIR}" --output-on-failure

"${BUILD_DIR}/skifflm" --version
"${BUILD_DIR}/skifflm" --help >/dev/null
"${BUILD_DIR}/skifflm" --doctor >/dev/null

if command -v clang-format >/dev/null 2>&1; then
    clang-format --dry-run --Werror \
        include/skifflm/*.hpp \
        src/*.cpp \
        tests/test_main.cpp
fi

echo
echo "Local CI checks passed."
