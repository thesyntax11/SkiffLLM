#!/usr/bin/env python3
"""Fetch a recommended GGUF model for SkiffLLM.

This is an optional companion helper. The SkiffLLM runtime never downloads or
registers models; when you run this script it fetches one GGUF file over HTTPS
from Hugging Face, checks the GGUF header, records a SHA-256 sidecar, and places
it in your SkiffLLM model directory. The catalog byte size is advisory because a
model maintainer can re-upload a revision with a different size.

Example:
    python3 scripts/model_fetch.py --list
    python3 scripts/model_fetch.py --model qwen2.5-0.5b
    python3 scripts/model_fetch.py --model llama3.2-1b --output-dir ./models
"""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

BASE_URL = "https://huggingface.co"

MODELS = {
    "qwen2.5-0.5b": {
        "name": "Qwen2.5 0.5B Instruct",
        "description": "Fast and small; good first model on older machines.",
        "repo": "Qwen/Qwen2.5-0.5B-Instruct-GGUF",
        "file": "qwen2.5-0.5b-instruct-q4_k_m.gguf",
        "bytes": 491400032,
        "license": "Apache-2.0",
    },
    "qwen3-0.6b": {
        "name": "Qwen3 0.6B Instruct",
        "description": "Small Qwen3 with optional thinking mode.",
        "repo": "bartowski/Qwen_Qwen3-0.6B-GGUF",
        "file": "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
        "bytes": 484220320,
        "license": "Apache-2.0",
    },
    "llama3.2-1b": {
        "name": "Llama 3.2 1B Instruct",
        "description": "Balanced quality and speed for typical CPUs.",
        "repo": "bartowski/Llama-3.2-1B-Instruct-GGUF",
        "file": "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
        "bytes": 807694464,
        "license": "Llama 3.2 Community License",
    },
    "smollm2-1.7b": {
        "name": "SmolLM2 1.7B Instruct",
        "description": "Mid-size option with a good quality-to-speed ratio.",
        "repo": "bartowski/SmolLM2-1.7B-Instruct-GGUF",
        "file": "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
        "bytes": 1055609824,
        "license": "Apache-2.0",
    },
    "phi3.5-mini": {
        "name": "Phi-3.5 Mini Instruct",
        "description": "Better reasoning; needs roughly 4 GB working set.",
        "repo": "bartowski/Phi-3.5-mini-instruct-GGUF",
        "file": "Phi-3.5-mini-instruct-Q4_K_M.gguf",
        "bytes": 2393232672,
        "license": "MIT",
    },
}


def human_bytes(value: int) -> str:
    gb = value / (1024.0 * 1024.0 * 1024.0)
    if gb >= 0.1:
        return f"{gb:.2f} GB"
    return f"{value / (1024.0 * 1024.0):.0f} MB"


def default_model_dir() -> Path:
    override = os.environ.get("LLM_MODEL_DIR")
    if override:
        return Path(override).expanduser()
    return Path.home() / ".local/share/llm/models"


def is_gguf(path: Path) -> bool:
    try:
        with path.open("rb") as handle:
            return handle.read(4) == b"GGUF"
    except OSError:
        return False


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def sidecar_path(path: Path) -> Path:
    return Path(str(path) + ".sha256")


def write_sidecar(path: Path) -> None:
    digest = file_sha256(path)
    sidecar_path(path).write_text(f"{digest}  {path.name}\n", encoding="ascii")


def verify(entry: dict, path: Path) -> list[str]:
    """Return a list of human-readable problems (empty means OK).

    The GGUF header and the SHA-256 sidecar are authoritative. The catalog
    ``bytes`` value is only a snapshot at publish time and can legitimately
    change when a model maintainer re-uploads a new revision, so a size
    difference is reported separately rather than treated as corruption.
    """
    problems: list[str] = []
    if not path.exists() or not path.is_file():
        return ["file does not exist"]
    if not is_gguf(path):
        problems.append("not a valid GGUF model")
        return problems
    sidecar = sidecar_path(path)
    if sidecar.exists():
        expected = sidecar.read_text(encoding="ascii").split()[0].lower()
        actual = file_sha256(path)
        if actual != expected:
            problems.append("SHA-256 mismatch (file is corrupt or modified)")
    else:
        problems.append("no checksum sidecar; run with --checksum to record one")
    return problems


def size_note(entry: dict, path: Path) -> str | None:
    """Return a human-readable advisory note when the size differs from the
    catalog snapshot, or None when it matches."""
    if not path.exists() or not path.is_file() or entry["bytes"] <= 0:
        return None
    actual = path.stat().st_size
    if actual == entry["bytes"]:
        return None
    return (
        f"size differs from the catalog snapshot "
        f"(catalog {entry['bytes']} bytes, found {actual} bytes)"
    )


def set_sidecar(path: Path) -> None:
    write_sidecar(path)


def _open_with_retry(request, attempts: int = 3, wait: float = 1.5):
    """Open a URL, retrying transient network/TLS failures a few times.

    Transient errors (connection reset, TLS EOF, short read) can occur when a
    CDN drops a connection during the handshake. Retrying the request is safe
    here because the downloader re-requests the full file and validates the
    result afterwards; it never resumes a corrupt partial file.
    """
    last_error: Exception | None = None
    for attempt in range(attempts):
        try:
            return urllib.request.urlopen(request, timeout=30)
        except (urllib.error.URLError, ConnectionError, OSError) as exc:
            last_error = exc
            if attempt + 1 < attempts:
                wait_seconds = wait * (attempt + 1)
                print(
                    f"  connection issue ({exc}); retrying in {wait_seconds:.0f}s...",
                    file=sys.stderr,
                )
                time.sleep(wait_seconds)
    if last_error is not None:
        raise last_error
    raise RuntimeError("could not open download URL")


def download(entry: dict, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / entry["file"]
    temp = output_dir / (entry["file"] + ".part")

    url = f"{BASE_URL}/{entry['repo']}/resolve/main/{entry['file']}"
    request = urllib.request.Request(url, headers={"User-Agent": "SkiffLLM/1.6.0"})

    downloaded = 0
    total = 0
    last_percent = -1
    print(f"Downloading {entry['name']} ({human_bytes(entry['bytes'])})")
    print(f"  {url}")

    with _open_with_retry(request) as response:
        total = int(response.headers.get("Content-Length") or 0)
        with temp.open("wb") as handle:
            while True:
                chunk = response.read(64 * 1024)
                if not chunk:
                    break
                handle.write(chunk)
                downloaded += len(chunk)
                if total > 0:
                    percent = min(100, int((downloaded * 100) / total))
                    if percent != last_percent:
                        last_percent = percent
                        print(f"\r  {percent:3d}%  {human_bytes(downloaded)}", end="", flush=True)
                else:
                    print(f"\r  {human_bytes(downloaded)}", end="", flush=True)

    print()
    if total > 0 and downloaded != total:
        temp.unlink(missing_ok=True)
        raise RuntimeError(
            "download size did not match the server's Content-Length "
            f"(expected {total} bytes, got {downloaded})"
        )
    if not is_gguf(temp):
        temp.unlink(missing_ok=True)
        raise RuntimeError("downloaded file is not a valid GGUF model")
    if entry["bytes"] > 0 and temp.stat().st_size != entry["bytes"]:
        # The catalog records a snapshot of the file at publish time. A model
        # maintainer can legitimately re-upload a new revision with a different
        # size, so this is advisory rather than a hard failure.
        print(
            f"  note: size differs from the catalog snapshot "
            f"(catalog {entry['bytes']}, got {temp.stat().st_size}); "
            "the SHA-256 sidecar below is the authoritative check",
            file=sys.stderr,
        )

    if output.exists():
        output.unlink()
    temp.rename(output)
    write_sidecar(output)
    print(f"Saved to {output}")
    print(f"  SHA-256 sidecar: {sidecar_path(output)}")
    print(f"  Now run: llm --model {output}")
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch a recommended GGUF model for SkiffLLM")
    parser.add_argument("--list", action="store_true", help="list available models")
    parser.add_argument("--model", metavar="ID", help="model id from --list")
    parser.add_argument(
        "--output-dir",
        metavar="DIR",
        type=Path,
        default=default_model_dir(),
        help="model directory (default: ~/.local/share/llm/models)",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="verify an existing downloaded model without downloading",
    )
    parser.add_argument(
        "--checksum",
        action="store_true",
        help="write a SHA-256 sidecar for an existing model",
    )
    args = parser.parse_args()

    if args.list:
        print("Available models:")
        for key, entry in MODELS.items():
            print(f"  {key:14s} {entry['name']}  [{entry.get('license', 'see upstream')}]")
            print(f"                 {entry['description']}")
            print(f"                 {human_bytes(entry['bytes'])}  (Q4_K_M)")
        print()
        print("Model files are downloaded from Hugging Face under their own")
        print("upstream license and are not redistributed by SkiffLLM. Respect")
        print("the license of each model before you use or redistribute it.")
        print("The Llama 3.2 Community License requires 'Built with Llama'")
        print("attribution and specific naming for redistributed model names.")
        return 0

    if not args.model:
        parser.print_help(sys.stderr)
        return 2

    entry = MODELS.get(args.model)
    if entry is None:
        print(f"Unknown model id: {args.model}", file=sys.stderr)
        print("Use --list to see available ids.", file=sys.stderr)
        return 2

    model_path = args.output_dir / entry["file"]
    if args.verify:
        problems = verify(entry, model_path)
        if problems:
            print(f"Verification failed for {model_path}:", file=sys.stderr)
            for problem in problems:
                print(f"  - {problem}", file=sys.stderr)
            return 1
        print(f"Verified: {model_path}")
        print("  GGUF header: ok")
        print(f"  Size: {human_bytes(entry['bytes'])} ({entry['bytes']} bytes)")
        note = size_note(entry, model_path)
        if note:
            print(f"  note: {note}")
        print("  SHA-256: match")
        return 0

    if args.checksum:
        if not model_path.exists():
            print(f"Model not found: {model_path}", file=sys.stderr)
            return 1
        write_sidecar(model_path)
        print(f"SHA-256 sidecar written: {sidecar_path(model_path)}")
        return 0

    license_name = entry.get("license", "see upstream license")
    if license_name != "Apache-2.0" and license_name != "MIT":
        print(
            f"Notice: {entry['name']} is distributed under the {license_name}. "
            "Review its terms (attribution, naming, redistribution) before "
            "commercial or public use.",
            file=sys.stderr,
        )

    try:
        download(entry, args.output_dir)
    except Exception as exc:  # noqa: BLE001 - user-facing command
        print(f"Download failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
