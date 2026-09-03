# Releasing

Releases follow semantic versioning. The version is set in:

- `CMakeLists.txt`
- `src/main.cpp`
- `docs/skiffllm.1`
- `android/app/build.gradle.kts` (`versionCode` and `versionName`)

Keep `CHANGELOG.md` current for every release.

## Local release archive

```bash
make release
scripts/ci-local.sh
scripts/release.sh
scripts/release.sh --help                          # all options
scripts/release.sh --output-dir artifacts --version 1.6.0
```

The script creates `skiffllm-<version>-<os>-<arch>.tar.gz` (for example
`skiffllm-1.6.0-linux-x86_64.tar.gz`, with `macos-arm64` on Apple Silicon and
`macos-x86_64` on Intel) containing the binary, licenses, docs, completions,
example config, logo, and helper scripts. It also emits a standalone native
binary named `skiffllm-<version>-<os>-<arch>[.exe]`, plus `checksums.txt`.

Once the archive is published as a GitHub release asset, users can install it
with:

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

> Keep the release archive and the published asset name identical. The helper
> maps `linux-x86_64`, `linux-aarch64`, `macos-x86_64`, `macos-arm64`, and
> `windows-x86_64` automatically.

## GitHub release

The repository's `main` branch must contain `.github/workflows/release.yml`.
Given Actions workflow permissions that allow write access (Settings → Actions
→ General → Workflow permissions → Read and write), tag a commit and push the
tag:

```bash
git tag -a v1.6.0 -m "SkiffLLM 1.6.0"
git push origin v1.6.0
```

The release workflow in `.github/workflows/release.yml` builds Linux, macOS,
and Windows desktop archives plus an Android debug APK, and publishes them as
GitHub Release assets automatically. Asset names are normalized so
`scripts/install-from-release.sh` can resolve them:

- `skiffllm-<version>-linux-x86_64.tar.gz`
- `skiffllm-<version>-macos-arm64.tar.gz`
- `skiffllm-<version>-windows-x86_64.zip`
- `skiffllm-<version>-linux-x86_64`
- `skiffllm-<version>-macos-arm64`
- `skiffllm-<version>-windows-x86_64.exe`
- `skiffllm-<version>-Android.apk`
- `skiffllm-<version>-iOS.ipa`
- `checksums.txt` (SHA-256 for every asset above)

The iOS job builds a device app through `ios/` and packages it with
`scripts/package-ios.sh`. It produces an unsigned `.ipa` when signing secrets
are absent. To publish a signed IPA, set `IOS_CERTIFICATE_P12_BASE64`,
`IOS_CERTIFICATE_PASSWORD`, `IOS_CERTIFICATE_IDENTITY`,
`IOS_DEVELOPMENT_TEAM`, `IOS_PROVISIONING_PROFILE_NAME`, and
`IOS_PROVISIONING_PROFILE_BASE64` in repository or environment secrets.

The same workflow can be launched manually from the Actions page to produce
downloadable artifacts without publishing a GitHub release; the `publish` job
only runs on tag pushes.

The workflow runs `scripts/release.sh` on the exact host platform it targets,
so the archive always contains a native binary. It does **not** cross-compile,
which means `linux-aarch64` and `macos-x86_64` assets are not produced by the
current workflow. If you need one, add a job that runs on an ARM64 Linux
runner or the `macos-15-intel` runner and run `bash scripts/release.sh`
there. Do not rename an `x86_64` binary as `aarch64` to fill a gap — that
would break the installer's checksum/name contract.

## Release checklist

- Run `scripts/ci-local.sh`.
- Confirm `--version` matches the release.
- Verify `--doctor`, `--model-info`, `--smoke`, and a real generation on a
  local model.
- Verify `--export`, `--attach`, `--serve`, and `--benchmark` on a local model.
- For Android releases, run `cd android && ./gradlew assembleDebug` and confirm
  the APK installs and loads a real GGUF model.
- For iOS, run `scripts/package-ios.sh` locally if you already have a device
  build; confirm the `.ipa` opens after signing.
- Update the changelog, app version, and docs.
- Push the tag.
