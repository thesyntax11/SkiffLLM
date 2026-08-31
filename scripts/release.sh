#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
VERSION="$("${BUILD_DIR}/skifflm" --version | awk '{print $2}')"

if [[ -z "${VERSION}" ]]; then
    echo "Could not determine the version. Build first."
    exit 2
fi

STAGE="${PROJECT_DIR}/staged"
ARCHIVE="skifflm-${VERSION}.tar.gz"
PLATFORM="$(uname -s)-$(uname -m)"

rm -rf "${STAGE}"
mkdir -p "${STAGE}/bin" "${STAGE}/share"

cp "${BUILD_DIR}/skifflm" "${STAGE}/bin/"
cp README.md LICENSE CHANGELOG.md SECURITY.md CONTRIBUTING.md "${STAGE}/"
cp -r docs "${STAGE}/share/"
cp -r scripts/completions "${STAGE}/share/"
cp configs/skifflm.example.conf "${STAGE}/share/"
cp -r scripts "${STAGE}/share/scripts"

tar -czf "${ARCHIVE}" -C "${STAGE}" .

echo
echo "Release archive: ${ARCHIVE}"
echo "Platform: ${PLATFORM}"
echo "Use: tar -xzf ${ARCHIVE} && ./bin/skifflm --help"
