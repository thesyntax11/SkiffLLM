#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
PREFIX=""
SOURCE_DIR=""
BACKEND="auto"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            PREFIX="${2:-}"
            shift 2
            ;;
        --source-dir)
            SOURCE_DIR="${2:-}"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="${2:-}"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="${2:-}"
            shift 2
            ;;
        --backend)
            BACKEND="${2:-}"
            shift 2
            ;;
        *)
            echo "Usage: scripts/setup.sh [--prefix <path>] [--source-dir <path>] [--build-dir <path>] [--build-type <type>] [--backend auto|cuda|metal|vulkan|opencl|blas|cpu]"
            exit 2
            ;;
    esac
done

mkdir -p "${BUILD_DIR}"
cd "${PROJECT_DIR}"

if [[ -n "${SOURCE_DIR}" ]]; then
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DLLM_LLAMA_SOURCE_DIR="${SOURCE_DIR}" \
        -DLLM_FETCH_LLAMA=OFF \
        -DLLM_LLAMA_BACKEND="${BACKEND}"
else
    cmake -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DLLM_LLAMA_BACKEND="${BACKEND}"
fi

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j

if [[ -n "${PREFIX}" ]]; then
    cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"
fi

echo
echo "SkiffLLM was built successfully."
echo "Run it with: ${BUILD_DIR}/llm --help"
echo "Run the tests with: ctest --test-dir ${BUILD_DIR} --output-on-failure"
