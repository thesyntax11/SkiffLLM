#!/usr/bin/env bash
set -euo pipefail

# Install a prebuilt SkiffLLM release archive.
#
# This is the script that a future `curl ... | sh` one-liner would fetch.
# It intentionally fails fast when a release archive is not published yet.
#
# Usage:
#   bash scripts/install-from-release.sh --version v1.6.0 --os linux --arch x86_64
#   curl -fsSL https://raw.githubusercontent.com/thesyntax11/SkiffLLM/main/scripts/install-from-release.sh | bash -s -- --version v1.6.0

VERSION=""
RAW_OS="$(uname -s | tr '[:upper:]' '[:lower:]')"
case "${RAW_OS}" in
    darwin) OS="darwin" ;;
    linux) OS="linux" ;;
    mingw*|msys*|cygwin*) OS="windows" ;;
    *) OS="${RAW_OS}" ;;
esac
RAW_ARCH="$(uname -m)"
case "${RAW_ARCH}" in
    amd64|x86_64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *) ARCH="${RAW_ARCH}" ;;
esac
PREFIX="${PREFIX:-${HOME}/.local}"
REPO="thesyntax11/SkiffLLM"

usage() {
    cat <<'EOF'
Usage: bash scripts/install-from-release.sh [options]

Options:
  --version VER      Required. For example: v1.6.0
  --os NAME          linux, darwin, windows (default: current OS)
  --arch NAME        x86_64, arm64, aarch64 (default: current arch)
  --prefix DIR       Install directory (default: $HOME/.local)
  --repo OWNER/NAME  GitHub repository (default: thesyntax11/SkiffLLM)
  --help             Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --os)
            OS="$2"
            shift 2
            ;;
        --arch)
            ARCH="$2"
            shift 2
            ;;
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --repo)
            REPO="$2"
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

if [[ -z "${VERSION}" ]]; then
    echo "error: --version is required" >&2
    usage >&2
    exit 2
fi

# Release archives are named with the bare version (e.g. "1.6.0") while the
# GitHub tag is prefixed (e.g. "v1.6.0"). Strip a leading "v" only from the
# asset filename; keep the full tag for the release/download URL.
ASSET_VERSION="${VERSION#v}"

case "${OS}-${ARCH}" in
    linux-x86_64)  ASSET="llm-${ASSET_VERSION}-linux-x86_64.tar.gz";;
    linux-aarch64|linux-arm64) ASSET="llm-${ASSET_VERSION}-linux-aarch64.tar.gz";;
    darwin-x86_64) ASSET="llm-${ASSET_VERSION}-macos-x86_64.tar.gz";;
    darwin-arm64)  ASSET="llm-${ASSET_VERSION}-macos-arm64.tar.gz";;
    windows-x86_64) ASSET="llm-${ASSET_VERSION}-windows-x86_64.zip";;
    *)
        echo "error: no release asset mapping for ${OS}-${ARCH}" >&2
        exit 2
        ;;
esac

URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET}"
echo "Downloading ${URL}" >&2
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

curl -fsSL --retry 3 -o "${TMP}/${ASSET}" "${URL}"

case "${ASSET}" in
    *.zip)
        # Extract into the same directory used for tar.gz so the binary is
        # always found at ${TMP}/bin/llm.
        command -v unzip >/dev/null 2>&1 || { echo "unzip is required" >&2; exit 2; }
        unzip -q "${TMP}/${ASSET}" -d "${TMP}"
        ;;
    *)
        tar -xzf "${TMP}/${ASSET}" -C "${TMP}"
        ;;
esac

BIN="${TMP}/bin/llm"
if [[ "${OS}" == "windows" ]]; then
    BIN="${TMP}/bin/llm.exe"
fi
if [[ ! -f "${BIN}" ]]; then
    echo "error: binary not found in the release archive" >&2
    exit 2
fi

install -d "${PREFIX}/bin"
install -m 0755 "${BIN}" "${PREFIX}/bin/llm"
echo "Installed ${PREFIX}/bin/llm"
