#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Dependency-free SkiffLLM server client.

Examples:
    python3 scripts/api_client.py
    python3 scripts/api_client.py --stream "Tell me a short joke."
    python3 scripts/api_client.py http://127.0.0.1:8080 --temperature 0.7 --prompt "Hello"
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request

DEFAULT_BASE = "http://127.0.0.1:8080"


def build_request(method: str, url: str, payload: dict | None = None):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    return urllib.request.Request(url, data=data, headers=headers, method=method)


def request(method: str, url: str, payload: dict | None = None, timeout: int = 120):
    req = build_request(method, url, payload)
    with urllib.request.urlopen(req, timeout=timeout) as response:
        body = response.read().decode("utf-8")
        return response.status, body


def stream_request(url: str, payload: dict, timeout: int = 120):
    req = build_request("POST", url, payload)
    response = urllib.request.urlopen(req, timeout=timeout)
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description="Call a SkiffLLM local server")
    parser.add_argument("base", nargs="?", default=DEFAULT_BASE, help="server base URL")
    parser.add_argument("--stream", action="store_true", help="stream tokens as SSE")
    parser.add_argument("--prompt", help="prompt text (falls back to positional text)")
    parser.add_argument("--model", default="skifflm", help="model id")
    parser.add_argument("--temperature", type=float, default=0.2, help="sampling temperature")
    parser.add_argument("--max-tokens", type=int, default=256, help="maximum generated tokens")
    parser.add_argument("text", nargs="*", help="prompt text")
    args = parser.parse_args()

    base = args.base.rstrip("/")

    status, body = request("GET", base + "/health", timeout=10)
    print(status, body.strip())
    if status != 200:
        return 1

    status, body = request("GET", base + "/v1/models", timeout=10)
    print(status, body.strip())
    if status != 200:
        return 1

    prompt = args.prompt or " ".join(args.text) or "Say hello."
    payload = {
        "model": args.model,
        "messages": [{"role": "user", "content": prompt}],
        "stream": args.stream,
        "temperature": args.temperature,
        "max_tokens": args.max_tokens,
    }

    if args.stream:
        print("POST", base + "/v1/chat/completions", "(streaming)")
        response = stream_request(base + "/v1/chat/completions", payload)
        try:
            for line in response:
                text = line.decode("utf-8", errors="replace").rstrip("\r\n")
                if not text:
                    continue
                if text.startswith("data:"):
                    text = text[5:].strip()
                if text == "[DONE]":
                    break
                try:
                    data = json.loads(text)
                    delta = data.get("choices", [{}])[0].get("delta", {})
                    content = delta.get("content", "")
                    if content:
                        print(content, end="", flush=True)
                except (json.JSONDecodeError, IndexError, AttributeError):
                    print(text)
        finally:
            response.close()
        print()
        return 0

    status, body = request("POST", base + "/v1/chat/completions", payload)
    print(status)
    print(body.strip())
    return 0 if status == 200 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (urllib.error.URLError, OSError) as exc:
        print("error: unable to reach SkiffLLM server:", exc, file=sys.stderr)
        sys.exit(1)
