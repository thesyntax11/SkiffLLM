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

"${BUILD_DIR}/skiffllm" --version
"${BUILD_DIR}/skiffllm" --help >/dev/null
"${BUILD_DIR}/skiffllm" --doctor >/dev/null
"${BUILD_DIR}/skiffllm" --doctor --network >/dev/null
"${BUILD_DIR}/skiffllm" model list >/dev/null
"${BUILD_DIR}/skiffllm" model info qwen2.5-0.5b >/dev/null
"${BUILD_DIR}/skiffllm" session list >/dev/null

bash -n scripts/release.sh
bash -n scripts/package-ios.sh
bash -n scripts/install-latest.sh

if command -v python3 >/dev/null 2>&1; then
    python3 -m py_compile scripts/model_fetch.py scripts/api_client.py
    python3 scripts/model_fetch.py --list >/dev/null
    python3 scripts/model_fetch.py --help >/dev/null
    python3 scripts/api_client.py --help >/dev/null
fi

# Keep the project version consistent across CMake, the C++ fallback, the man
# page, and the Android app.
EXPECTED_VERSION="$(sed -n 's/project(SkiffLLM VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
if [[ -z "${EXPECTED_VERSION}" ]]; then
    echo "Unable to determine project version from CMakeLists.txt" >&2
    exit 1
fi
grep -q "SKIFFLLM_VERSION \"${EXPECTED_VERSION}\"" src/main.cpp
grep -q "SkiffLLM ${EXPECTED_VERSION}" docs/skiffllm.1
grep -q "versionName = \"${EXPECTED_VERSION}\"" android/app/build.gradle.kts

if command -v clang-format >/dev/null 2>&1; then
    clang-format --dry-run --Werror \
        include/skiffllm/*.hpp \
        src/*.cpp \
        tests/test_main.cpp
fi

echo
echo "Local CI checks passed."
