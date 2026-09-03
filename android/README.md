# SkiffLLM for Android

A native Android client for SkiffLLM. It embeds llama.cpp through JNI and runs
a GGUF model entirely on the device. The app uses `INTERNET` **only** when you
download a recommended GGUF model from Hugging Face over HTTPS. Inference,
chat, export, and file loading are fully offline; no telemetry is sent.

## Requirements

- Android Studio 2024.1 or newer
- Android SDK Platform 34
- Android NDK 26.3.11579264
- CMake 3.22 or newer (bundled with SDK)
- JDK 17
- A GGUF model file already on the device

## Build

Open the `android/` directory in Android Studio and sync Gradle:

```bash
cd android
./gradlew assembleDebug
```

If Gradle is not installed locally, open the project in Android Studio and
let it download the pinned Gradle version. The APK is written to
`app/build/outputs/apk/debug/app-debug.apk`.

### Build without downloading llama.cpp

The native build fetches the same pinned llama.cpp revision when no local
checkout is supplied. To use an existing checkout, add a Gradle property:

```bash
./gradlew assembleDebug \
  -Pskifflm.llamaSourceDir=/path/to/llama.cpp
```

Or add to `android/gradle.properties`:

```properties
skifflm.llamaSourceDir=/path/to/llama.cpp
```

## Supported ABIs

- `arm64-v8a` (physical devices; Apple Silicon-era and most modern phones)
- `x86_64` (Android emulators)

32-bit ARM (`armeabi-v7a`) is intentionally not shipped. The pinned
llama.cpp sgemm kernel requires memory-mapped FP16 NEON intrinsics that
ARMv7 does not provide, and quantized LLM inference on 32-bit ARM is not
practical with the supported model sizes.

## Load a model

Option A — download a recommended model:

1. Open SkiffLLM.
2. Tap `Settings` then `Models`.
3. Pick a quantized model from the catalog and tap `Download`.
4. The app downloads it over HTTPS, verifies the GGUF header, records a
   `.gguf.sha256` sidecar alongside it, stores it in internal storage, and
   loads it automatically.

Option B — load your own `.gguf`:

1. Copy a `.gguf` file to the phone. The simplest path is to put it in
   Downloads.
2. Open SkiffLLM.
3. Tap `Settings`, `Models`, then `Browse device`.
4. Pick the GGUF file from the file picker.
5. The app copies it into internal storage, verifies the GGUF header, and only
   then loads it.

The app calculates token counts, generation time, and tokens per second from
real inference, and shows a live context-usage progress bar under the composer.

## Theme

`Settings -> Theme` selects Dark, Light, or System. The choice is persisted
locally and applies immediately.

## GPU/NPU acceleration

Native GPU acceleration is opt-in because the toolchain must be configured
before the shared library is built:

```bash
./gradlew assembleDebug -Pskifflm.backend=vulkan   # or opencl
```

The desktop equivalent is `-DSKIFFLLM_LLAMA_BACKEND=vulkan|opencl`. The app
keeps `--gpu-layers` equivalent under `Settings -> GPU layers`.

## Recommended models

See [models.md](models.md) for the recommended small, quantized models and the
RAM they need. Start with a Q4_K_M model in the 0.5B to 1.7B range.

## Share into SkiffLLM

Any app that shares text can send it straight into SkiffLLM. Android's share
sheet lists SkiffLLM as a target; tapping it opens the app with a `Shared from
another app` banner. Tap `Use` to fill the input, then ask your question as
usual. The shared text stays on the device.

## Conversations

The top bar exposes a `Chats` button. There you can create,
open, rename, export, import, and delete local conversations. Each
conversation is stored as a JSON file in app-private storage; switching loads
that conversation immediately and the active name is shown next to the
SkiffLLM title. Each conversation also remembers its own sampling settings and
code-mode toggle, so switching back restores exactly how you set that chat up.
The `Export`/`Import` buttons write or read a `.json` backup that includes the
system prompt, messages, sampling settings, and code mode; import always
creates a new conversation and never overwrites an existing one.

## Attach a text file

Tap `Attach` above the composer to pick a text, JSON, or XML file. The app
reads it locally (UTF-8, capped at 64 KB), shows a note when the file is
truncated, and puts the content into the composer so it is sent as part of the
next user message. The file never leaves the device.

## Quick prompts

Save a prompt you reuse in `Settings -> Quick prompts`. It appears as a tap-to-
insert chip above the composer and is stored locally.

## Sampling profiles

`Settings -> Profile presets` applies a balanced, fast, creative, code, or
precise sampling profile immediately. The chosen values remain fully editable
under the ordinary sampling fields.

## Code mode

`Settings -> Code mode` toggles safe-code behavior: the app instructs the model
to propose concrete, reviewable edits as unified diffs and never to claim that
a file was changed. It mirrors the desktop `--code` mode; applying the proposal
remains a manual, human step.

## Warm-up and benchmark

`Settings -> Warm up model` re-runs a short decode pass on the loaded model to
reduce first-answer latency after a long session. `Settings -> Run 3-round
benchmark` performs three real generations and reports measured generated
tokens, average round time, and average tokens per second, exactly like the
desktop `--benchmark` flow. No numbers are fabricated.

## Session statistics

Every completed generation updates a local session counter shown under the
context bar: messages, prompt/generated tokens, total generation time, and
average tokens per second. This mirrors the desktop `/stats` command and is
reset when you clear or switch conversations.

## Stop sequences

`Settings -> Stop sequences` lets you add one or more strings that end the
generation as soon as the model emits them. Stop sequences are local and are
passed to the native engine on every turn (they are intentionally disabled
during conversation compaction).

## Compact conversation

The Compose settings panel includes `Compact conversation`. It sends the
current transcript back through the loaded model (at `temperature 0.2`) and
replaces the conversation with a compact bullet summary, preserving facts,
decisions, preferences, and unfinished work.

## Persistent facts

`Settings -> Persistent facts` keeps facts like *"user prefers concise
answers"*. They are injected on every turn and never leave the phone.

## Settings

- Model catalog with one-tap Hugging Face downloads, progress, cancel, use,
  and delete
- Context size
- Threads
- GPU layers
- Chat template override
- Temperature
- Top-p
- Top-k
- Min-p
- Typical-p
- Repeat penalty
- Repeat last N
- Max generated tokens
- Sampling profile presets (balanced/fast/creative/code/precise)
- Code mode (unified-diff proposals)
- System prompt
- Persistent facts
- Quick prompts
- Stop sequences
- Conversation compaction
- Stop generation
- Copy last answer
- Regenerate the last answer
- Clear conversation
- Export conversation
- Attach text/JSON/XML file

## Runtime behavior

- Inference runs on a background single thread so the interface stays
  responsive.
- Each loaded model is warmed with one short pass to reduce first-answer
  latency.
- Downloads run on a separate thread so chat and local model loading stay
  usable while a GGUF is downloading.
- Download progress is shown per model and can be cancelled.
- Download checks free storage before starting, verifies the expected size and
  GGUF header, writes a local `.gguf.sha256` sidecar, and rejects incomplete
  files before they become usable.
- The last successfully loaded model is remembered and reloaded on the next
  launch.
- Conversations, the system prompt, and sampling/load settings are saved
  locally and restored on the next launch.
- Thread count defaults to the device CPU count (capped at 8).
- Stop signals a cancellation request to the native generator.
- Conversation export creates a Markdown document and opens the Android share
  sheet. This is offline; it never uploads anything.
- Text, JSON, and XML files can be attached via the system file picker and are
  read locally as UTF-8 (capped at 64 KB).

## Network privacy

The only Android permission is `INTERNET`, used exclusively for HTTPS downloads
from Hugging Face. The app does not send prompts, conversation history, model
names, analytics, or crash reports to any server.

## Performance honesty

- Actual speed depends on the phone, the model size, and quantization.
- No speed claims are included because they would vary per device.
- For a smooth experience on older phones, use a 0.5B to 1B Q4_K_M model and a
  small context window.
