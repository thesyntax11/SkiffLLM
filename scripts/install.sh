#!/usr/bin/env bash
set -euo pipefail

# One-command local install for source checkouts.
# Produces build/Release/skifflm and stages a small shared install tree.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

PREFIX="${PREFIX:-${HOME}/.local}"
CMAKE="${CMAKE:-cmake}"

echo "Configuring Release build..."
${CMAKE} --preset release 2>/dev/null \
    || ${CMAKE} -S . -B build/Release -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_BUILD_TESTS=ON

echo "Building..."
${CMAKE} --build build/Release -j

echo "Installing to ${PREFIX}/bin ..."
install -d "${PREFIX}/bin"
install -m 0755 build/Release/skifflm "${PREFIX}/bin/skifflm"

echo
echo "Installed: ${PREFIX}/bin/skifflm"
echo "Try:       skifflm --version"
