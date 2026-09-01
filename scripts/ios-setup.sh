#!/usr/bin/env bash
set -euo pipefail

# One-command iOS setup for a Mac with Xcode + XcodeGen.
#
# It fetches the pinned llama.cpp revision and builds an XCFramework for the
# iOS device and iOS simulator with Metal enabled, then generates the Xcode
# project with XcodeGen. Open ios/SkiffLLM.xcodeproj and run.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IOS_DIR="${PROJECT_DIR}/ios"
THIRD_PARTY="${IOS_DIR}/third_party"
LLAMA_DIR="${THIRD_PARTY}/llama"
LLAMA_TAG="85c55223caf0a2ad0d1d88e5a73ab3fe36107867"

cd "${IOS_DIR}"
mkdir -p "${THIRD_PARTY}"

echo "==> Preparing llama.cpp (${LLAMA_TAG})"
if [[ ! -d "${LLAMA_DIR}/.git" ]]; then
    git clone https://github.com/ggml-org/llama.cpp.git "${LLAMA_DIR}"
fi
if ! git -C "${LLAMA_DIR}" cat-file -e "${LLAMA_TAG}^{commit}" 2>/dev/null; then
    git -C "${LLAMA_DIR}" fetch origin "${LLAMA_TAG}" \
        || git -C "${LLAMA_DIR}" fetch origin
fi
git -C "${LLAMA_DIR}" checkout --detach "${LLAMA_TAG}"

if [[ ! -f "${LLAMA_DIR}/build-apple/llama.xcframework/Info.plist" ]]; then
    echo "==> Building llama.xcframework (device + simulator, Metal enabled)"
    pushd "${LLAMA_DIR}" >/dev/null
    bash build-xcframework.sh ios-sim ios-device
    popd >/dev/null
fi

echo "==> Generating Xcode project"
if ! command -v xcodegen >/dev/null 2>&1; then
    echo
    echo "XcodeGen is not installed. Install it with:"
    echo "  brew install xcodegen"
    echo "Then re-run: scripts/ios-setup.sh"
    exit 1
fi
xcodegen generate --spec project.yml

echo
echo "iOS project ready."
echo "Open:  open ${IOS_DIR}/SkiffLLM.xcodeproj"
echo "Build: xcodebuild -project SkiffLLM.xcodeproj -scheme SkiffLLM -destination 'generic/platform=iOS' build"
