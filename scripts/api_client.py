# -*- coding: utf-8 -*-
import json
import sys
import urllib.error
import urllib.request

DEFAULT_BASE = "http://127.0.0.1:8080"


def request(method, url, payload=None, timeout=120):
    data = None
    headers = {}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as response:
        body = response.read().decode("utf-8")
        return response.status, body


def main():
    base = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_BASE
    base = base.rstrip("/")

    status, body = request("GET", base + "/health", timeout=10)
    print(status, body.strip())
    if status != 200:
        return 1

    status, body = request("GET", base + "/v1/models", timeout=10)
    print(status, body.strip())
    if status != 200:
        return 1

    prompt = " ".join(sys.argv[2:]) or "Say hello."
    payload = {
        "model": "skifflm",
        "messages": [{"role": "user", "content": prompt}],
        "stream": False,
        "temperature": 0.2,
    }
    status, body = request("POST", base + "/v1/chat/completions", payload)
    print(status)
    print(body.strip())
    return 0 if status == 200 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (urllib.error.URLError, OSError) as exc:
        print("error: unable to reach SkiffLLM server at", DEFAULT_BASE, exc)
        sys.exit(1)
