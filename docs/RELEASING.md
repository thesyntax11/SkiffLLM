# Releasing

Releases follow semantic versioning. The version is set in:

- `CMakeLists.txt`
- `src/main.cpp`
- `docs/skifflm.1`

Keep `CHANGELOG.md` current for every release.

## Local release archive

```bash
scripts/ci-local.sh
scripts/release.sh
```

The script creates `skifflm-VERSION.tar.gz` with the binary, licenses, docs,
completions, example config, and helper scripts.

## GitHub release

When workflow permissions are available on the repository, tag a commit and
push the tag:

```bash
git tag -a v1.4.0 -m "SkiffLLM 1.4.0"
git push origin v1.4.0
```

The release workflow in `.github/workflows/release.yml` builds Linux and macOS
archives and publishes them automatically.

## Release checklist

- Run `scripts/ci-local.sh`.
- Confirm `--version` matches the release.
- Verify `--doctor`, `--model-info`, `--smoke`, and a real generation on a
  local model.
- Verify `--export`, `--attach`, `--serve`, and `--benchmark` on a local model.
- Update the changelog and docs.
- Push the tag.
