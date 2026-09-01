# SkiffLLM for iOS

A native SwiftUI iOS client that runs a GGUF model on-device through a small
Objective-C++ bridge over llama.cpp. It is offline-first: the only network use
is an optional HTTPS model download from Hugging Face. Prompts, conversations,
and generated text stay on the device.

> This directory is source + setup tooling. It writes a real
> `.xcodeproj` on a Mac; it is not built in the Linux sandbox. The native
> bridge compiles the same llama.cpp APIs and sampler chain used by the
> desktop engine.

## Requirements

- macOS 13+ with Xcode 15+
- CMake 3.28+ (`brew install cmake`)
- XcodeGen (`brew install xcodegen`)
- iOS 16.4+ device or simulator

## One-command setup

```bash
bash scripts/ios-setup.sh
```

The script:

1. Clones the pinned llama.cpp revision into `ios/third_party/llama`.
2. Builds `llama.xcframework` for `ios-sim` and `ios-device` with Metal.
3. Generates `ios/SkiffLLM.xcodeproj` with XcodeGen.

Then open the project:

```bash
open ios/SkiffLLM.xcodeproj
```

Choose a device/simulator and Run. If you only want the simulator, edit
`project.yml` to keep only the simulator header search path, or pass
`-destination 'generic/platform=iOS Simulator'`.

## Features

- SwiftUI chat with token-by-token streaming.
- Real measured context-usage bar (`used/capacity tokens`, %) and
  token/time/tok-per-second stats after each response.
- Dark / Light / System theme.
- Multiple local conversations (create / open / delete).
- Persistent facts injected into the system prompt every turn.
- Custom quick-prompt chips above the composer.
- Hugging Face model download with progress and CANCEL/DOWNLOAD, verified by
  GGUF magic header + size + SHA-256 sidecar, then loaded automatically.
- Import a local GGUF from the Files app; the GGUF header is verified before
  the model is loaded.
- Share text to SkiffLLM from another app through the built-in Share
  Extension; the main app shows it as a “Shared from another app” banner.
- Attach a text, JSON, or XML file with the paperclip button. The file is read
  locally as UTF-8 (capped at 64 KB), with a notice when it is truncated, and
  inserted into the composer for the next message.
- Export the current conversation as Markdown through the iOS share sheet, or
  copy the last assistant answer to the clipboard with the copy button.
- Model warm-up after loading to reduce first-answer latency.
- Model description + active backend shown in the header.
- Sampling controls match the desktop/Android clients: profiles
  (balanced/fast/creative/code/precise), temperature, top-p, top-k, min-p,
  typical-p, repeat penalty/last-N, max tokens, seed, and stop sequences.
- Conversation compaction (`Settings -> Compact conversation`) sends the
  current transcript back through the loaded model at `temperature 0.2` and
  replaces the conversation with a compact bullet summary, preserving facts,
  decisions, preferences, and unfinished work. Stop sequences are disabled
  during compaction.
- Safe `Code mode` (`Settings`) mirrors the desktop `--code` mode: the model is
  told to propose concrete, reviewable unified diffs and never to claim a file
  was changed. Applying the proposal stays a manual step.
- `Settings -> Warm up model` re-runs a short decode pass on the loaded model;
  `Settings -> Run 3-round benchmark` reports measured tokens, average round
  time, and tokens per second (real measurements, mirroring desktop
  `--benchmark`, never fabricated).
- A live session counter under the context bar tracks messages, prompt and
  generated tokens, and average tokens per second, mirroring desktop `/stats`.
- Keep the same short catalog IDs as the desktop CLI.

## Models

Use `Models` in the app to download a recommended Q4_K_M GGUF, use “Import a
GGUF from Files” to copy a local file into the app, or place your own `.gguf`
into the app's Application Support `models/` folder and pick it via `Use`.
Downloads are auto-loaded after validation.

## Notes

- GPU acceleration uses Metal. To offload layers to the Apple GPU, set
  `GPU layers` in `Settings` (default `0` = CPU-only).
- The Share Extension and the app use the app group `group.com.skifflm.app`.
  For device builds set your Development Team in Xcode so the group can be
  provisioned; the simulator usually works with a team signed to the group.
- The app requests no telemetry and sends no analytics. The only outgoing
  request is the explicit Hugging Face download you trigger, and the Share
  Extension only writes received text into the shared app group.
