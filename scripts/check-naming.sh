#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}"

violations=0

fail() {
    echo "NAMING ERROR: $1" >&2
    violations=$((violations + 1))
}

for file in \
    "include/llm" \
    "docs/llm.1" \
    "configs/llm.example.conf" \
    "scripts/completions/llm.bash" \
    "scripts/completions/llm.fish" \
    "scripts/completions/llm.zsh" \
    "packaging/windows/llm.ico" \
    "packaging/windows/llm.rc" \
    "packaging/windows/llm.png"; do
    if [[ -e "${file}" ]]; then
        fail "forbidden legacy path exists: ${file}"
    fi
done

for dir in \
    "android/app/src/main/java/com/llm" \
    "android/app/src/main/java/com/skiffllm"; do
    if [[ ! -d "${dir}" && -e "${dir}" ]]; then
        fail "forbidden legacy path exists: ${dir}"
    fi
done

while IFS= read -r file; do
    if grep -Eq \
        -e 'com/llm' \
        -e 'com\.llm' \
        -e 'group\.com\.llm' \
        -e 'Java_com_llm_' \
        -e '-Pllm\.' \
        -e '(^|[^A-Za-z0-9_])llm_core' \
        -e '(^|[^A-Za-z0-9_])llm_tests' \
        -e '(^|[^A-Za-z0-9_])llm_android' \
        -e '(^|[^A-Za-z0-9_])llm_socket_t' \
        -e '(^|[^A-Za-z0-9_])llm_settings' \
        -e 'include/llm' \
        -e 'docs/llm\.' \
        -e 'configs/llm\.' \
        -e 'scripts/completions/llm\.' \
        -e 'packaging/windows/llm\.' "${file}" 2>/dev/null; then
        fail "${file} still contains a legacy llm name"
    fi
done < <(find . -type f \
    ! -path './.git/*' \
    ! -path './build/*' \
    ! -path './build-*/*' \
    ! -path './.venv/*' \
    ! -path './node_modules/*' \
    ! -path './scripts/check-naming.sh' \
    ! -name '*.gguf' \
    ! -name '*.bin' \
    ! -name '*.skif' \
    ! -name '*.tar.gz' \
    ! -name '*.zip' \
    ! -name '*.exe' \
    ! -name '*.ico' \
    ! -name '*.png' \
    ! -name '*.jpg' \
    ! -name '*.jpeg' \
    | sort)

for path in \
    "include/skiffllm" \
    "docs/skiffllm.1" \
    "configs/skiffllm.example.conf" \
    "scripts/completions/skiffllm.bash" \
    "scripts/completions/skiffllm.fish" \
    "scripts/completions/skiffllm.zsh" \
    "packaging/windows/skiffllm.ico" \
    "packaging/windows/skiffllm.rc" \
    "packaging/windows/skiffllm.png" \
    "android/app/src/main/java/com/skiffllm"; do
    if [[ ! -e "${path}" ]]; then
        fail "required skiffllm path is missing: ${path}"
    fi
done

if [[ "${violations}" -gt 0 ]]; then
    echo "Found ${violations} naming violation(s)." >&2
    exit 1
fi

echo "All shipped names are skiffllm."
