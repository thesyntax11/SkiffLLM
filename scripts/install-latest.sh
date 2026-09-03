#!/usr/bin/env bash
set -euo pipefail

REPO="thesyntax11/SkiffLLM"
BRANCH="main"
PREFIX="${PREFIX:-${HOME}/.local}"

usage() {
    cat <<'EOF'
Usage: bash scripts/install-latest.sh [options]

Options:
  --prefix DIR   Install directory (default: $HOME/.local)
  --repo OWNER/NAME  GitHub repository (default: thesyntax11/SkiffLLM)
  --help         Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
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

latest_tag() {
    local api_output
    api_output="$(curl -fsSL --retry 3 "https://api.github.com/repos/${REPO}/releases/latest")"
    if command -v python3 >/dev/null 2>&1; then
        python3 -c 'import json,sys; print(json.loads(sys.argv[1])["tag_name"])' "${api_output}"
    else
        sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' <<<"${api_output}"
    fi
}

VERSION="$(latest_tag)"
if [[ -z "${VERSION}" ]]; then
    echo "error: no release found for ${REPO}" >&2
    exit 1
fi

curl -fsSL --retry 3 "https://raw.githubusercontent.com/${REPO}/${BRANCH}/scripts/install-from-release.sh" |
    bash -s -- --version "${VERSION}" --prefix "${PREFIX}" --repo "${REPO}"
