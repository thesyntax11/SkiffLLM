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
PLATFORM="$(uname -s)-$(uname -m)"

rm -rf "${STAGE}"
mkdir -p "${STAGE}/bin" "${STAGE}/share"

cp "${BUILD_DIR}/skifflm" "${STAGE}/bin/"
cp README.md LICENSE CHANGELOG.md SECURITY.md CONTRIBUTING.md "${STAGE}/"
cp -r docs "${STAGE}/share/"
cp -r scripts/completions "${STAGE}/share/"
cp configs/skifflm.example.conf "${STAGE}/share/"
cp -r scripts "${STAGE}/share/scripts"

ARCHIVE="skifflm-${VERSION}-${PLATFORM}.tar.gz"
tar -czf "${ARCHIVE}" -C "${STAGE}" .

# Android APK, when present, joins the same bundle.
if [[ -f "${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk" ]]; then
    cp "${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk" \
        "${PROJECT_DIR}/SkiffLLM-${VERSION}-Android.apk"
fi

# Always publish a checksum file for the archives in this directory.
rm -f "${PROJECT_DIR}/checksums.txt"
for asset in "skifflm-${VERSION}-"*.tar.gz "SkiffLLM-${VERSION}-"*.apk; do
    if [[ -f "${asset}" ]]; then
        sha256sum "${asset}" >> "${PROJECT_DIR}/checksums.txt"
    fi
done

echo
echo "Release archive: ${ARCHIVE}"
echo "Platform:        ${PLATFORM}"
echo "Checksums:       ${PROJECT_DIR}/checksums.txt"
echo "Use:             tar -xzf ${ARCHIVE} && ./bin/skifflm --help"
