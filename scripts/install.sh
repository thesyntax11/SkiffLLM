#!/usr/bin/env bash
set -euo pipefail

# One-command local install for a source checkout.
#
# Examples:
#   bash scripts/install.sh
#   bash scripts/install.sh --prefix "$HOME/.local" --backend auto --skip-tests
#   PREFIX=/usr/local bash scripts/install.sh

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-${HOME}/.local}"
BACKEND="${BACKEND:-auto}"
CMAKE="${CMAKE:-cmake}"
SKIP_TESTS=0

usage() {
    cat <<'EOF'
Usage: bash scripts/install.sh [options]

Options:
  --prefix DIR      Install under DIR (default: $HOME/.local)
  --backend NAME    auto, cpu, cuda, metal, vulkan, opencl or blas
  --skip-tests      Do not run CTest after building
  --help            Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --backend)
            BACKEND="$2"
            shift 2
            ;;
        --skip-tests)
            SKIP_TESTS=1
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

if ! command -v "${CMAKE}" >/dev/null 2>&1; then
    echo "error: '${CMAKE}' is required but was not found in PATH" >&2
    echo "Install CMake 3.20+ and add it to PATH, then retry." >&2
    exit 1
fi
if ! command -v ctest >/dev/null 2>&1 && [[ "${SKIP_TESTS}" -eq 0 ]]; then
    echo "warning: ctest not found; tests will be skipped" >&2
    SKIP_TESTS=1
fi

cd "${PROJECT_DIR}"

BUILD_DIR="build/${BACKEND}"
if [[ "${BACKEND}" == "auto" ]]; then
    BUILD_DIR="build/Release"
    echo "Configuring Release build (backend: auto)..."
    "${CMAKE}" --preset release
else
    echo "Configuring Release build (backend: ${BACKEND})..."
    "${CMAKE}" -S . -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLM_BUILD_TESTS=ON \
        -DLLM_LLAMA_BACKEND="${BACKEND}"
fi

echo "Building (${BUILD_DIR})..."
"${CMAKE}" --build "${BUILD_DIR}" --config Release -j

if [[ "${SKIP_TESTS}" -eq 0 ]]; then
    echo "Running tests..."
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi

BIN_DIR="${PREFIX}/bin"
SHARE_DIR="${PREFIX}/share/llm"
MAN_DIR="${PREFIX}/share/man/man1"
DOC_DIR="${PREFIX}/share/doc/llm"
COMPLETION_DIR="${PREFIX}/share/llm/completions"

install -d "${BIN_DIR}" "${SHARE_DIR}" "${MAN_DIR}" "${DOC_DIR}" "${COMPLETION_DIR}"
install -m 0755 "${BUILD_DIR}/llm" "${BIN_DIR}/llm"
install -m 0644 README.md LICENSE CHANGELOG.md SECURITY.md \
    "${DOC_DIR}/"
install -m 0644 docs/*.md "${DOC_DIR}/"
install -m 0644 docs/llm.1 "${MAN_DIR}/llm.1"
install -m 0644 configs/llm.example.conf "${SHARE_DIR}/llm.example.conf"
install -m 0644 scripts/completions/* "${COMPLETION_DIR}/"

echo
echo "Installed SkiffLLM:"
echo "  Binary:       ${BIN_DIR}/llm"
echo "  Man page:     ${MAN_DIR}/llm.1"
echo "  Docs:         ${DOC_DIR}"
echo "  Completions:  ${COMPLETION_DIR}"
echo
echo "Add ${BIN_DIR} to PATH to use it from any shell."
echo "Try: ${BIN_DIR}/llm --version"
