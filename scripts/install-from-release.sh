#!/usr/bin/env bash
set -euo pipefail

# Install a prebuilt SkiffLLM release archive.
#
# This is the script that a future `curl ... | sh` one-liner would fetch.
# It intentionally fails fast when a release archive is not published yet.
#
# Usage:
#   bash scripts/install-from-release.sh --version v1.9.0 --os linux --arch x86_64
#   curl -fsSL https://raw.githubusercontent.com/thesyntax11/SkiffLLM/main/scripts/install-from-release.sh | bash -s -- --version v1.9.0

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
VERIFY_SHA=1

usage() {
    cat <<'EOF'
Usage: bash scripts/install-from-release.sh [options]

Options:
  --version VER      Required. For example: v1.9.0
  --os NAME          linux, darwin, windows (default: current OS)
  --arch NAME        x86_64, arm64, aarch64 (default: current arch)
  --prefix DIR       Install directory (default: $HOME/.local)
  --repo OWNER/NAME  GitHub repository (default: thesyntax11/SkiffLLM)
  --no-checksum      Skip the .sha256 sidecar verification
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
        --no-checksum)
            VERIFY_SHA=0
            shift
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

# Release archives are named with the bare version (e.g. "1.9.0") while the
# GitHub tag is prefixed (e.g. "v1.9.0"). Strip a leading "v" only from the
# asset filename; keep the full tag for the release/download URL.
ASSET_VERSION="${VERSION#v}"

case "${OS}-${ARCH}" in
    linux-x86_64)  ASSET="skiffllm-${ASSET_VERSION}-linux-x86_64.tar.gz";;
    linux-aarch64|linux-arm64) ASSET="skiffllm-${ASSET_VERSION}-linux-aarch64.tar.gz";;
    darwin-x86_64) ASSET="skiffllm-${ASSET_VERSION}-macos-x86_64.tar.gz";;
    darwin-arm64)  ASSET="skiffllm-${ASSET_VERSION}-macos-arm64.tar.gz";;
    windows-x86_64) ASSET="skiffllm-${ASSET_VERSION}-windows-x86_64.zip";;
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

if [[ "${VERIFY_SHA}" -eq 1 ]]; then
    SUM_URL="${URL}.sha256"
    if curl -fsSL --retry 3 -o "${TMP}/${ASSET}.sha256" "${SUM_URL}"; then
        PYTHON="python3"
        command -v "${PYTHON}" >/dev/null 2>&1 || PYTHON="python"
        if ! command -v "${PYTHON}" >/dev/null 2>&1; then
            echo "warning: Python 3 not found; skipping checksum verification" >&2
        else
        "${PYTHON}" - "${TMP}/${ASSET}" "${TMP}/${ASSET}.sha256" <<'PY'
import hashlib
import sys

asset = sys.argv[1]
sidecar = sys.argv[2]
expected = open(sidecar, encoding="utf-8").read().split()[0]
digest = hashlib.sha256()
with open(asset, "rb") as fh:
    for chunk in iter(lambda: fh.read(1 << 20), b""):
        digest.update(chunk)
actual = digest.hexdigest()
if actual != expected:
    print("checksum mismatch", file=sys.stderr)
    print("expected", expected, file=sys.stderr)
    print("actual  ", actual, file=sys.stderr)
    sys.exit(1)
PY
            echo "Checksum verified for ${ASSET}" >&2
        fi
    else
        echo "warning: .sha256 sidecar not found; skipping verification" >&2
    fi
fi

case "${ASSET}" in
    *.zip)
        # Extract into the same directory used for tar.gz so the binary is
        # always found at ${TMP}/bin/skiffllm.
        command -v unzip >/dev/null 2>&1 || { echo "unzip is required" >&2; exit 2; }
        unzip -q "${TMP}/${ASSET}" -d "${TMP}"
        ;;
    *)
        tar -xzf "${TMP}/${ASSET}" -C "${TMP}"
        ;;
esac

BIN="${TMP}/bin/skiffllm"
BIN_NAME="skiffllm"
if [[ "${OS}" == "windows" ]]; then
    BIN="${TMP}/bin/skiffllm.exe"
    BIN_NAME="skiffllm.exe"
fi
if [[ ! -f "${BIN}" ]]; then
    echo "error: binary not found in the release archive" >&2
    exit 2
fi

install -d "${PREFIX}/bin" \
    "${PREFIX}/share/skiffllm/web" \
    "${PREFIX}/share/skiffllm/completions" \
    "${PREFIX}/share/doc/skiffllm" \
    "${PREFIX}/share/man/man1"
install -m 0755 "${BIN}" "${PREFIX}/bin/${BIN_NAME}"
if [[ -f "${TMP}/bin/skiffllm-cli" ]]; then
    install -m 0755 "${TMP}/bin/skiffllm-cli" "${PREFIX}/bin/skiffllm-cli"
elif [[ -f "${TMP}/bin/skiffllm-cli.exe" ]]; then
    install -m 0755 "${TMP}/bin/skiffllm-cli.exe" "${PREFIX}/bin/skiffllm-cli.exe"
fi

if [[ -d "${TMP}/share/skiffllm/web" ]]; then
    cp -R "${TMP}/share/skiffllm/web/." "${PREFIX}/share/skiffllm/web/"
fi
if [[ -f "${TMP}/share/skiffllm.example.conf" ]]; then
    install -m 0644 "${TMP}/share/skiffllm.example.conf" \
        "${PREFIX}/share/skiffllm/skiffllm.example.conf"
fi
if [[ -d "${TMP}/share/completions" ]]; then
    cp -R "${TMP}/share/completions/." "${PREFIX}/share/skiffllm/completions/"
fi
if [[ -d "${TMP}/share/docs" ]]; then
    rm -rf "${PREFIX}/share/doc/skiffllm"
    cp -R "${TMP}/share/docs" "${PREFIX}/share/doc/skiffllm"
fi
if [[ -f "${PREFIX}/share/doc/skiffllm/skiffllm.1" ]]; then
    install -m 0644 "${PREFIX}/share/doc/skiffllm/skiffllm.1" \
        "${PREFIX}/share/man/man1/skiffllm.1"
fi
for doc in README.md LICENSE CHANGELOG.md SECURITY.md CONTRIBUTING.md; do
    if [[ -f "${TMP}/${doc}" ]]; then
        install -m 0644 "${TMP}/${doc}" "${PREFIX}/share/doc/skiffllm/${doc}"
    fi
done

echo "Installed ${PREFIX}/bin/${BIN_NAME}"
echo "Installed shared files under ${PREFIX}/share/skiffllm"
