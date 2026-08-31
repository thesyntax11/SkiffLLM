# SkiffLLM for Android

A native Android client for SkiffLLM. It embeds llama.cpp through JNI and runs
a GGUF model entirely on the device. The app has **no INTERNET permission**, so
it cannot connect to the network at runtime.

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

1. Copy a `.gguf` file to the phone. The simplest path is to put it in
   Downloads.
2. Open SkiffLLM.
3. Tap `Settings` then `Load model`.
4. Pick the GGUF file from the file picker.
5. The app copies it into internal storage and loads it.

The app does not download models. It calculates token counts, generation time,
and tokens per second from real inference.

## Recommended models

See [models.md](models.md) for the recommended small, quantized models and the
RAM they need. Start with a Q4_K_M model in the 0.5B to 1.7B range.

## Settings

- Context size
- Threads
- GPU layers
- Chat template override
- Temperature
- Top-p
- Top-k
- Max generated tokens
- System prompt
- Stop generation
- Clear conversation
- Export conversation

## Runtime behavior

- Inference runs on a background single thread so the interface stays
  responsive.
- Tokens stream into the conversation as they are produced.
- The last successfully loaded model is remembered and reloaded on the next
  launch.
- Stop signals a cancellation request to the native generator.
- Conversation export creates a Markdown document and opens the Android share
  sheet. This is offline; it never uploads anything.

## Performance honesty

- Actual speed depends on the phone, the model size, and quantization.
- No speed claims are included because they would vary per device.
- For a smooth experience on older phones, use a 0.5B to 1B Q4_K_M model and a
  small context window.
