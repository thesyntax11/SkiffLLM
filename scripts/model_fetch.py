#!/usr/bin/env python3
"""Fetch a recommended GGUF model for SkiffLLM.

This is an optional companion helper. The SkiffLLM runtime never downloads or
registers models; when you run this script it fetches one GGUF file over HTTPS
from Hugging Face, verifies the GGUF header and download size, and places it in
your SkiffLLM model directory.

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
    },
    "qwen3-0.6b": {
        "name": "Qwen3 0.6B Instruct",
        "description": "Small Qwen3 with optional thinking mode.",
        "repo": "bartowski/Qwen_Qwen3-0.6B-GGUF",
        "file": "Qwen_Qwen3-0.6B-Q4_K_M.gguf",
        "bytes": 484220320,
    },
    "llama3.2-1b": {
        "name": "Llama 3.2 1B Instruct",
        "description": "Balanced quality and speed for typical CPUs.",
        "repo": "bartowski/Llama-3.2-1B-Instruct-GGUF",
        "file": "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
        "bytes": 807694464,
    },
    "smollm2-1.7b": {
        "name": "SmolLM2 1.7B Instruct",
        "description": "Mid-size option with a good quality-to-speed ratio.",
        "repo": "bartowski/SmolLM2-1.7B-Instruct-GGUF",
        "file": "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
        "bytes": 1055609824,
    },
    "phi3.5-mini": {
        "name": "Phi-3.5 Mini Instruct",
        "description": "Better reasoning; needs roughly 4 GB working set.",
        "repo": "bartowski/Phi-3.5-mini-instruct-GGUF",
        "file": "Phi-3.5-mini-instruct-Q4_K_M.gguf",
        "bytes": 2393232672,
    },
}


def human_bytes(value: int) -> str:
    gb = value / (1024.0 * 1024.0 * 1024.0)
    if gb >= 0.1:
        return f"{gb:.2f} GB"
    return f"{value / (1024.0 * 1024.0):.0f} MB"


def default_model_dir() -> Path:
    override = os.environ.get("SKIFFLLM_MODEL_DIR")
    if override:
        return Path(override).expanduser()
    return Path.home() / ".local/share/skifflm/models"


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


def check_size(entry: dict, path: Path) -> bool:
    actual = path.stat().st_size
    return actual == entry["bytes"]


def verify(entry: dict, path: Path) -> list[str]:
    """Return a list of human-readable problems (empty means OK)."""
    problems: list[str] = []
    if not path.exists() or not path.is_file():
        return ["file does not exist"]
    if not is_gguf(path):
        problems.append("not a valid GGUF model")
    if not check_size(entry, path):
        problems.append(
            f"size mismatch: expected {entry['bytes']} bytes, found {path.stat().st_size}"
        )
    sidecar = sidecar_path(path)
    if sidecar.exists():
        expected = sidecar.read_text(encoding="ascii").split()[0].lower()
        actual = file_sha256(path)
        if actual != expected:
            problems.append("SHA-256 mismatch (file is corrupt or modified)")
    else:
        problems.append("no checksum sidecar; run with --checksum to record one")
    return problems


def set_sidecar(path: Path) -> None:
    write_sidecar(path)


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

    with urllib.request.urlopen(request, timeout=30) as response:
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
    if total > 0 and downloaded < total:
        temp.unlink(missing_ok=True)
        raise RuntimeError("download ended before the expected Content-Length")
    if not is_gguf(temp):
        temp.unlink(missing_ok=True)
        raise RuntimeError("downloaded file is not a valid GGUF model")

    if output.exists():
        output.unlink()
    temp.rename(output)
    write_sidecar(output)
    print(f"Saved to {output}")
    print(f"  SHA-256 sidecar: {sidecar_path(output)}")
    print(f"  Now run: skifflm --model {output}")
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
        help="model directory (default: ~/.local/share/skifflm/models)",
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
            print(f"  {key:14s} {entry['name']}")
            print(f"                 {entry['description']}")
            print(f"                 {human_bytes(entry['bytes'])}  (Q4_K_M)")
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
        print("  SHA-256: match")
        return 0

    if args.checksum:
        if not model_path.exists():
            print(f"Model not found: {model_path}", file=sys.stderr)
            return 1
        write_sidecar(model_path)
        print(f"SHA-256 sidecar written: {sidecar_path(model_path)}")
        return 0

    try:
        download(entry, args.output_dir)
    except Exception as exc:  # noqa: BLE001 - user-facing command
        print(f"Download failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
