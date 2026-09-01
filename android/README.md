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

- `arm64-v8a`
- `armeabi-v7a`
- `x86_64` (emulators)

## Load a model

Option A — download a recommended model:

1. Open SkiffLLM.
2. Tap `Settings` then `Models`.
3. Pick a quantized model from the catalog and tap `Download`.
4. The app downloads it over HTTPS, verifies the GGUF header, stores it in
   internal storage, and loads it automatically.

Option B — load your own `.gguf`:

1. Copy a `.gguf` file to the phone. The simplest path is to put it in
   Downloads.
2. Open SkiffLLM.
3. Tap `Settings`, `Models`, then `Browse device`.
4. Pick the GGUF file from the file picker.
5. The app copies it into internal storage and loads it.

The app calculates token counts, generation time, and tokens per second from
real inference.

## Recommended models

See [models.md](models.md) for the recommended small, quantized models and the
RAM they need. Start with a Q4_K_M model in the 0.5B to 1.7B range.

## Share into SkiffLLM

Any app that shares text can send it straight into SkiffLLM. Android's share
sheet lists SkiffLLM as a target; tapping it opens the app with a `Shared from
another app` banner. Tap `Use` to fill the input, then ask your question as
usual. The shared text stays on the device.

## Quick prompts

Save a prompt you reuse in `Settings -> Quick prompts`. It appears as a tap-to-
insert chip above the composer and is stored locally.

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
- System prompt
- Persistent facts
- Quick prompts
- Stop generation
- Clear conversation
- Export conversation

## Runtime behavior

- Inference runs on a background single thread so the interface stays
  responsive.
- Each loaded model is warmed with one short pass to reduce first-answer
  latency.
- Downloads run on a separate thread so chat and local model loading stay
  usable while a GGUF is downloading.
- Download progress is shown per model and can be cancelled.
- Download checks free storage before starting, validates the GGUF header,
  and rejects incomplete files before they become usable.
- The last successfully loaded model is remembered and reloaded on the next
  launch.
- Conversations, the system prompt, and sampling/load settings are saved
  locally and restored on the next launch.
- Thread count defaults to the device CPU count (capped at 8).
- Stop signals a cancellation request to the native generator.
- Conversation export creates a Markdown document and opens the Android share
  sheet. This is offline; it never uploads anything.

## Network privacy

The only Android permission is `INTERNET`, used exclusively for HTTPS downloads
from Hugging Face. The app does not send prompts, conversation history, model
names, analytics, or crash reports to any server.

## Performance honesty

- Actual speed depends on the phone, the model size, and quantization.
- No speed claims are included because they would vary per device.
- For a smooth experience on older phones, use a 0.5B to 1B Q4_K_M model and a
  small context window.
