#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is not installed; skipping format check."
    exit 0
fi

echo "Checking C++ formatting..."
clang-format --dry-run --Werror \
    include/skiffllm/*.hpp \
    src/*.cpp \
    tests/test_main.cpp \
    tests/test_extra.cpp

echo "C++ formatting is clean."
