#!/usr/bin/env bash
set -euo pipefail

# Produce reproducible local release archives from a built tree.
#
# Examples:
#   bash scripts/release.sh                         # find build/release/skifflm
#   bash scripts/release.sh --build-dir build/debug
#   bash scripts/release.sh --output artifacts/
#
# Supported host platforms:
#   linux-x86_64   linux-aarch64   macos-x86_64   macos-arm64   windows-x86_64
# On Windows this script is intended to run in Git Bash (the default shell used
# by GitHub Actions when a step declares `shell: bash`).

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR=""
OUTPUT_DIR=""
VERSION=""

usage() {
    cat <<'EOF'
Usage: bash scripts/release.sh [options]

Options:
  --build-dir DIR   Path to the CMake build tree containing the binary
  --output-dir DIR  Directory for the generated archive and checksums
  --version VER     Override the version (default: from the binary)
  --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

cd "${PROJECT_DIR}"
BUILD_DIR="${BUILD_DIR:-build/release}"
OUTPUT_DIR="${OUTPUT_DIR:-${PROJECT_DIR}}"

# GitHub-hosted Windows runners expose `python` and usually not `python3`;
# pick whichever alias is available.
PYTHON="python3"
if ! command -v "${PYTHON}" >/dev/null 2>&1; then
    PYTHON="python"
fi
if ! command -v "${PYTHON}" >/dev/null 2>&1; then
    echo "error: Python 3 is required to create checksums/zip archives" >&2
    exit 2
fi

BINARY="${BUILD_DIR}/skifflm"
if [[ ! -x "${BINARY}" ]]; then
    if [[ -x "${BUILD_DIR}/skifflm.exe" ]]; then
        BINARY="${BUILD_DIR}/skifflm.exe"
    elif [[ -x "${BUILD_DIR}/Release/skifflm.exe" ]]; then
        BINARY="${BUILD_DIR}/Release/skifflm.exe"
    elif [[ -x "${BUILD_DIR}/Release/skifflm" ]]; then
        BINARY="${BUILD_DIR}/Release/skifflm"
    else
        echo "error: binary not found at ${BINARY}" >&2
        echo "Build first with: make release or cmake --build build/release" >&2
        exit 2
    fi
fi

if [[ -z "${VERSION}" ]]; then
    VERSION="$("${BINARY}" --version | awk '{print $2}')"
fi
if [[ -z "${VERSION}" ]]; then
    echo "error: could not determine version" >&2
    exit 2
fi

STAGE="${OUTPUT_DIR}/staged"
mkdir -p "${STAGE}/bin" "${STAGE}/share"

BINARY_NAME="skifflm"
if [[ "${BINARY}" == *.exe ]]; then
    BINARY_NAME="skifflm.exe"
fi
cp "${BINARY}" "${STAGE}/bin/${BINARY_NAME}"
cp README.md LICENSE CHANGELOG.md SECURITY.md CONTRIBUTING.md "${STAGE}/"
cp -r docs "${STAGE}/share/"
cp -r scripts/completions "${STAGE}/share/"
cp configs/skifflm.example.conf "${STAGE}/share/"
cp -r scripts "${STAGE}/share/scripts"

# Normalize the asset name to the same `<os>-<arch>` convention that
# scripts/install-from-release.sh looks for. darwin is published as "macos".
# Release archives are produced on the host that runs this script, so they use
# the current platform; cross-builds should be produced on each target OS.
RAW_OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "${RAW_OS}" in
    darwin) OS_NAME="macos" ;;
    linux) OS_NAME="linux" ;;
    mingw*|msys*|cygwin*) OS_NAME="windows" ;;
    *)
        echo "error: unrecognized host OS '${RAW_OS}'" >&2
        exit 2
        ;;
esac

ARCH_NAME="$(uname -m)"
case "${ARCH_NAME}" in
    amd64) ARCH_NAME="x86_64" ;;
    arm64) ARCH_NAME="arm64" ;;
    aarch64) ARCH_NAME="aarch64" ;;
    x86_64) ARCH_NAME="x86_64" ;;
    *)
        echo "error: unrecognized host architecture '${ARCH_NAME}'" >&2
        exit 2
        ;;
esac
if [[ "${OS_NAME}" == "windows" ]]; then
    # Git Bash reports x86_64 on a 64-bit Windows runner. Keep the architecture
    # string that install-from-release.sh already maps for Windows.
    ARCH_NAME="x86_64"
fi
PLATFORM="${OS_NAME}-${ARCH_NAME}"

# Fail fast if the installer helper has no mapping for this asset name.
case "${PLATFORM}" in
    linux-x86_64|linux-aarch64|macos-x86_64|macos-arm64|windows-x86_64) ;;
    *)
        echo "error: no release asset mapping for platform ${PLATFORM}; " \
             "produce the archive on a supported host" >&2
        exit 2
        ;;
esac

sha256_of() {
    "${PYTHON}" - "$1" <<'PY'
import hashlib
import os
import sys

path = sys.argv[1]
digest = hashlib.sha256()
with open(path, "rb") as fh:
    for chunk in iter(lambda: fh.read(1 << 20), b""):
        digest.update(chunk)
print("%s  %s" % (digest.hexdigest(), os.path.basename(path)))
PY
}

if [[ "${OS_NAME}" == "windows" ]]; then
    ARCHIVE="${OUTPUT_DIR}/skifflm-${VERSION}-${PLATFORM}.zip"
    rm -f "${ARCHIVE}"
    "${PYTHON}" - "${STAGE}" "${ARCHIVE}" <<'PY'
import shutil
import sys

stage = sys.argv[1]
archive = sys.argv[2]
base = archive[: -len(".zip")]
shutil.make_archive(base, "zip", root_dir=stage)
PY
else
    ARCHIVE="${OUTPUT_DIR}/skifflm-${VERSION}-${PLATFORM}.tar.gz"
    rm -f "${ARCHIVE}"
    tar -czf "${ARCHIVE}" -C "${STAGE}" .
fi

if [[ -f "${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk" ]]; then
    cp "${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk" \
        "${OUTPUT_DIR}/SkiffLLM-${VERSION}-Android.apk"
fi

rm -f "${OUTPUT_DIR}/checksums.txt"
for asset in "${OUTPUT_DIR}/skifflm-${VERSION}-"*.tar.gz \
             "${OUTPUT_DIR}/skifflm-${VERSION}-"*.zip \
             "${OUTPUT_DIR}/SkiffLLM-${VERSION}-"*.apk; do
    if [[ -f "${asset}" ]]; then
        sha256_of "${asset}" >> "${OUTPUT_DIR}/checksums.txt"
    fi
done

echo
echo "Release archive: ${ARCHIVE}"
echo "Platform:        ${PLATFORM}"
echo "Checksums:       ${OUTPUT_DIR}/checksums.txt"
echo "Use:             tar -xzf $(basename "${ARCHIVE}") && ./bin/${BINARY_NAME} --help"
