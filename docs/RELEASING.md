# Releasing

Releases follow semantic versioning. The version is set in:

- `CMakeLists.txt`
- `src/main.cpp`
- `docs/skifflm.1`
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

The script creates `skifflm-<version>-<os>-<arch>.tar.gz` (for example
`skifflm-1.6.0-linux-x86_64.tar.gz`, with `macos-arm64` on Apple Silicon and
`macos-x86_64` on Intel) containing the binary, licenses, docs, completions,
example config, logo, and helper scripts, plus `checksums.txt`.

Once the archive is published as a GitHub release asset, users can install it
with:

```bash
bash scripts/install-from-release.sh --version v1.6.0
```

> Keep the release archive and the published asset name identical. The helper
> maps `linux-x86_64`, `linux-aarch64`, `macos-x86_64`, `macos-arm64`, and
> `windows-x86_64` automatically.

## GitHub release

When workflow permissions are available on the repository, tag a commit and
push the tag:

```bash
git tag -a v1.4.0 -m "SkiffLLM 1.4.0"
git push origin v1.4.0
```

The release workflow in `.github/workflows/release.yml` builds Linux, macOS,
and Windows archives plus an Android APK, and publishes them automatically.
Asset names are normalized to `skifflm-<version>-<os>-<arch>.tar.gz` (`.zip` on
Windows) so `scripts/install-from-release.sh` can resolve them. The
`linux-aarch64` entry runs on an ARM64 hosted runner; if that runner is not
available on your plan, remove that matrix entry rather than shipping an
x86_64 binary under an aarch64 name.

## Release checklist

- Run `scripts/ci-local.sh`.
- Confirm `--version` matches the release.
- Verify `--doctor`, `--model-info`, `--smoke`, and a real generation on a
  local model.
- Verify `--export`, `--attach`, `--serve`, and `--benchmark` on a local model.
- For Android releases, run `cd android && ./gradlew assembleDebug` and confirm
  the APK installs and loads a real GGUF model.
- Update the changelog, app version, and docs.
- Push the tag.
