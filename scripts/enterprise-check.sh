#!/usr/bin/env bash
# Non-destructive enterprise preflight for a SkiffLLM install.
#
# Checks what actually exists: the binary, a GGUF model, the SHA-256 sidecar,
# the config file, free disk space, and (when a model is present) the linked
# backends. It never downloads, never generates output, and never modifies
# files.
#
# Usage:
#   scripts/enterprise-check.sh [--bin PATH] [--model FILE] [--config FILE]

set -u

usage() {
    cat <<'EOF'
Usage: scripts/enterprise-check.sh [--bin PATH] [--model FILE] [--config FILE]

Options:
  --bin PATH      Path to the skiffllm binary (default: auto-detect).
  --model FILE    GGUF model to check (default: env/config/model-dir).
  --config FILE   Config file to validate with --show-config.
EOF
}

BIN="${SKIFFLLM_BIN:-}"
MODEL="${SKIFFLLM_MODEL:-}"
CONFIG="${SKIFFLLM_CONFIG:-}"

while [ $# -gt 0 ]; do
    case "$1" in
        --bin) BIN="${2:-}"; shift 2 ;;
        --model) MODEL="${2:-}"; shift 2 ;;
        --config) CONFIG="${2:-}"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

pass=0
warn=0
fail=0

pass() { pass=$((pass + 1)); echo "  [PASS] $1"; }
warn() { warn=$((warn + 1)); echo "  [WARN] $1"; }
fail() { fail=$((fail + 1)); echo "  [FAIL] $1"; }

section() { echo; echo "== $1 =="; }

# ---- Binary ------------------------------------------------------------
section "Binary"
if [ -z "$BIN" ]; then
    for candidate in \
        ./build/release/skiffllm \
        ./build/skiffllm \
        "$HOME/.local/bin/skiffllm" \
        /usr/local/bin/skiffllm; do
        if [ -x "$candidate" ]; then BIN="$candidate"; break; fi
    done
fi
if [ -z "$BIN" ] && command -v skiffllm >/dev/null 2>&1; then
    BIN="$(command -v skiffllm)"
fi

if [ -z "$BIN" ]; then
    fail "skiffllm binary not found (set SKIFFLLM_BIN or pass --bin)"
else
    pass "binary found: $BIN"
    if ! "$BIN" --version >/dev/null 2>&1; then
        fail "binary is not runnable: $BIN"
    else
        version="$("$BIN" --version 2>&1 | head -n1)"
        pass "version: $version"
    fi
fi

# ---- Model --------------------------------------------------------------
section "Model"
if [ -z "$MODEL" ] && [ -n "${SKIFFLLM_MODEL_DIR:-}" ]; then
    for f in "$SKIFFLLM_MODEL_DIR"/*.gguf; do
        [ -f "$f" ] && MODEL="$f" && break
    done
fi
if [ -z "$MODEL" ] && [ -n "$CONFIG" ] && [ -r "$CONFIG" ]; then
    MODEL="$(grep -E '^model=' "$CONFIG" | head -n1 | cut -d= -f2-)"
fi

if [ -z "$MODEL" ]; then
    if [ -n "$BIN" ]; then
        # Try the default model directory with --list-models (no model needed).
        listed="$("$BIN" --list-models 2>&1 || true)"
        if echo "$listed" | grep -q '\.gguf'; then
            warning_hint="$(echo "$listed" | grep '\.gguf' | head -n1)"
            warn "model not specified; examples found: $warning_hint"
        else
            warn "no model specified and none found (pass --model or set SKIFFLLM_MODEL)"
        fi
    else
        warn "no model specified (pass --model or set SKIFFLLM_MODEL)"
    fi
else
    if [ ! -f "$MODEL" ]; then
        fail "model file does not exist: $MODEL"
    elif [ ! -r "$MODEL" ]; then
        fail "model file is not readable: $MODEL"
    else
        size="$(du -h "$MODEL" | cut -f1)"
        pass "model readable: $MODEL ($size)"
        if [ -f "${MODEL}.sha256" ]; then
            pass "SHA-256 sidecar present: ${MODEL}.sha256"
        else
            warn "no SHA-256 sidecar; record one with: model_fetch.py --checksum or model verify --update"
        fi
    fi
fi

# ---- Config --------------------------------------------------------------
section "Config"
if [ -z "$CONFIG" ] && [ -r "$HOME/.config/skiffllm/config" ]; then
    CONFIG="$HOME/.config/skiffllm/config"
fi
if [ -z "$CONFIG" ]; then
    warn "no config file given; defaults will be used"
elif [ ! -r "$CONFIG" ]; then
    fail "config file not readable: $CONFIG"
elif [ -z "$BIN" ]; then
    warn "cannot validate config without a binary"
else
    if "$BIN" --show-config --config "$CONFIG" >/dev/null 2>&1; then
        pass "config parses: $CONFIG"
    else
        fail "config failed --show-config: $CONFIG"
    fi
fi

# ---- Disk ----------------------------------------------------------------
section "Disk"
target="$HOME"
[ -n "$MODEL" ] && target="$(dirname "$MODEL")"
if [ -d "$target" ]; then
    avail="$(df -h "$target" 2>/dev/null | awk 'NR==2 {print $4}')"
    if [ -n "$avail" ]; then
        pass "free space on $target: $avail available"
    else
        warn "could not determine free space for $target"
    fi
else
    warn "directory not found for disk check: $target"
fi

# ---- Backends -------------------------------------------------------------
section "Backends"
if [ -z "$BIN" ]; then
    warn "cannot inspect backends without a binary"
elif [ -z "$MODEL" ]; then
    warn "backend inspection needs a model file (--model)"
else
    report="$("$BIN" --model "$MODEL" --backend-info 2>&1 || true)"
    if echo "$report" | grep -q 'Backends linked'; then
        pass "backend report from binary"
        echo "$report" | sed 's/^/      /'
    else
        warn "backend info unavailable (build may be CPU-only or a model load failed)"
        echo "$report" | head -n3 | sed 's/^/      /'
    fi
fi

echo
echo "Summary: $pass passed, $warn warnings, $fail failures."
if [ "$fail" -gt 0 ]; then
    exit 1
fi
exit 0
