#!/usr/bin/env bash
set -euo pipefail

APP=""
VERSION=""
OUTPUT_DIR="artifacts"

usage() {
    cat <<'EOF'
Usage: bash scripts/package-ios.sh --app <path> --version <version> [--output-dir dir]

Options:
  --app PATH       Path to a built SkiffLLM.app bundle
  --version VER    Release version, for example 1.6.0
  --output-dir DIR Output directory (default: artifacts)
  --help           Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --app)
            APP="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
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

if [[ -z "${APP}" ]]; then
    echo "error: --app is required" >&2
    exit 2
fi
if [[ -z "${VERSION}" ]]; then
    echo "error: --version is required" >&2
    exit 2
fi
if [[ ! -d "${APP}" ]]; then
    echo "error: app bundle not found: ${APP}" >&2
    exit 2
fi

mkdir -p "${OUTPUT_DIR}"
STAGE="${OUTPUT_DIR}/Payload"
rm -rf "${STAGE}" "${OUTPUT_DIR}/llm-${VERSION}-iOS.ipa" \
    "${OUTPUT_DIR}/llm-${VERSION}-iOS.app.tar.gz"
mkdir -p "${STAGE}"
cp -R "${APP}" "${STAGE}/SkiffLLM.app"

if [[ -n "${LLM_SIGNING_IDENTITY:-}" ]]; then
    find "${STAGE}/SkiffLLM.app" -depth -name "*.appex" -print0 |
        while IFS= read -r -d '' extension; do
            codesign --force --deep --sign "${LLM_SIGNING_IDENTITY}" "${extension}"
        done
    codesign --force --deep --sign "${LLM_SIGNING_IDENTITY}" "${STAGE}/SkiffLLM.app"
fi

ditto -c -k --keepParent "${STAGE}" "${OUTPUT_DIR}/llm-${VERSION}-iOS.ipa"
tar -czf "${OUTPUT_DIR}/llm-${VERSION}-iOS.app.tar.gz" -C "${OUTPUT_DIR}" Payload
rm -rf "${STAGE}"

echo "iOS package: ${OUTPUT_DIR}/llm-${VERSION}-iOS.ipa"
echo "iOS archive: ${OUTPUT_DIR}/llm-${VERSION}-iOS.app.tar.gz"
