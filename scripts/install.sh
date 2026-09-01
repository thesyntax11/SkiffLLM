#!/usr/bin/env bash
set -euo pipefail

# One-command local install for source checkouts.
# Produces build/Release/skifflm and stages a small shared install tree.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

PREFIX="${PREFIX:-${HOME}/.local}"
CMAKE="${CMAKE:-cmake}"
BACKEND="${BACKEND:-auto}"

echo "Configuring Release build (backend: ${BACKEND})..."
if [[ "${BACKEND}" == "auto" ]]; then
    ${CMAKE} --preset release 2>/dev/null \
        || ${CMAKE} -S . -B build/Release -DCMAKE_BUILD_TYPE=Release -DSKIFFLLM_BUILD_TESTS=ON
else
    ${CMAKE} -S . -B "build/Release-${BACKEND}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSKIFFLLM_BUILD_TESTS=ON \
        -DSKIFFLLM_LLAMA_BACKEND="${BACKEND}"
    BUILD_DIR="build/Release-${BACKEND}"
fi

BUILD_DIR="${BUILD_DIR:-build/Release}"
echo "Building (${BUILD_DIR})..."
${CMAKE} --build "${BUILD_DIR}" -j

echo "Installing to ${PREFIX}/bin ..."
install -d "${PREFIX}/bin"
install -m 0755 "${BUILD_DIR}/skifflm" "${PREFIX}/bin/skifflm"

echo
echo "Installed: ${PREFIX}/bin/skifflm"
echo "Try:       skifflm --version"
