#!/usr/bin/env bash
set -euo pipefail

# Local Android CI. Requires ANDROID_HOME / Android Studio SDK and a JDK 17.
# The Gradle wrapper downloads the pinned Gradle version; the native build
# fetches the pinned llama.cpp revision unless SKIFFLLM_LLAMA_SOURCE_DIR is set.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${PROJECT_DIR}/android"

if [[ -z "${ANDROID_HOME:-}" ]] && [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
    echo "error: ANDROID_HOME or ANDROID_SDK_ROOT is not set." >&2
    exit 1
fi

LLAMA_ARGS=()
if [[ -n "${SKIFFLLM_LLAMA_SOURCE_DIR:-}" ]]; then
    LLAMA_ARGS+=("-Pskiffllm.llamaSourceDir=${SKIFFLLM_LLAMA_SOURCE_DIR}")
fi
if [[ -n "${SKIFFLLM_BACKEND:-}" ]]; then
    LLAMA_ARGS+=("-Pskiffllm.backend=${SKIFFLLM_BACKEND}")
fi

./gradlew --stacktrace assembleDebug "${LLAMA_ARGS[@]}"

echo
echo "Android local CI check passed."
echo "APK: ${PROJECT_DIR}/android/app/build/outputs/apk/debug/app-debug.apk"
