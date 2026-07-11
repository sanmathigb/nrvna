#!/bin/bash
# Courtesy wrapper over nrvnad's own lifecycle interface.
# The daemon owns the truth; this file just launches and delegates.

set -euo pipefail

NRVNA_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NRVNA_BUILD_DIR="${NRVNA_BUILD_DIR:-$(cd "$NRVNA_LIB_DIR/.." && pwd)/build}"
: "${NRVNA_START_TIMEOUT:=120}"
: "${NRVNA_LOG_DIR:=/tmp}"

nrvna__bin() {
    if [ -n "${NRVNA_DAEMON_BIN:-}" ] && [ -x "${NRVNA_DAEMON_BIN}" ]; then echo "$NRVNA_DAEMON_BIN"
    elif [ -x "$NRVNA_BUILD_DIR/nrvnad" ]; then echo "$NRVNA_BUILD_DIR/nrvnad"
    else command -v nrvnad; fi
}

nrvna_status() { "$(nrvna__bin)" status "$1" >/dev/null 2>&1; }

nrvna_stop() { "$(nrvna__bin)" stop "$1"; }

nrvna_start() {
    local model="$1" ws="$2"; shift 2
    # status exit codes: 0 ready, 2 starting (adopt, don't double-launch), 1 absent
    local code=0 launcher="" log=""
    "$(nrvna__bin)" status "$ws" >/dev/null 2>&1 || code=$?
    [ "$code" -eq 0 ] && return 0
    if [ "$code" -ne 2 ]; then
        mkdir -p "$ws"
        log="${NRVNA_LOG_DIR%/}/nrvna-$(basename "$ws").log"
        "$(nrvna__bin)" "$model" "$ws" "$@" >"$log" 2>&1 &
        launcher=$!
    fi
    local waited=0
    while ! nrvna_status "$ws"; do
        if [ -n "$launcher" ] && ! kill -0 "$launcher" 2>/dev/null; then
            echo "nrvna-lib: daemon exited during startup (log: $log)" >&2; tail -5 "$log" >&2; return 1
        fi
        [ "$waited" -ge "$NRVNA_START_TIMEOUT" ] && { echo "nrvna-lib: startup timeout" >&2; return 1; }
        sleep 1; waited=$((waited + 1))
    done
}

nrvna_ensure() { nrvna_start "$@"; }
