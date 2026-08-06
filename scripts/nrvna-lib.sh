#!/bin/bash
# Courtesy wrapper over nrvnad's own lifecycle interface.
# The daemon owns state. This file starts it and sends control commands.

NRVNA_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NRVNA_BUILD_DIR="${NRVNA_BUILD_DIR:-$(cd "$NRVNA_LIB_DIR/.." && pwd)/build}"
: "${NRVNA_START_TIMEOUT:=120}"
: "${NRVNA_LOG_DIR:=/tmp}"

nrvna__bin() {
    if [ -n "${NRVNA_DAEMON_BIN:-}" ] && [ -x "${NRVNA_DAEMON_BIN}" ]; then echo "$NRVNA_DAEMON_BIN"
    elif [ -x "$NRVNA_BUILD_DIR/nrvnad" ]; then echo "$NRVNA_BUILD_DIR/nrvnad"
    else command -v nrvnad; fi
}

nrvna_status() {
    [ "$#" -eq 1 ] || {
        echo "usage: nrvna_status <workspace>" >&2
        return 2
    }
    local daemon
    daemon="$(nrvna__bin)" || return 1
    "$daemon" status "$1" >/dev/null 2>&1
}

nrvna_stop() {
    [ "$#" -eq 1 ] || {
        echo "usage: nrvna_stop <workspace>" >&2
        return 2
    }
    local daemon
    daemon="$(nrvna__bin)" || return 1
    "$daemon" stop "$1"
}

nrvna_start() {
    [ "$#" -ge 2 ] || {
        echo "usage: nrvna_start <model> <workspace> [nrvnad options]" >&2
        return 2
    }
    case "$NRVNA_START_TIMEOUT" in
        ''|*[!0-9]*)
            echo "nrvna-lib: NRVNA_START_TIMEOUT must be a nonnegative integer" >&2
            return 2
            ;;
    esac
    local model="$1" ws="$2"; shift 2
    local code=0 daemon launcher="" log=""
    daemon="$(nrvna__bin)" || {
        echo "nrvna-lib: nrvnad not found" >&2
        return 1
    }
    # Exit 0 means ready. Exit 2 means starting; use that process.
    "$daemon" status "$ws" >/dev/null 2>&1 || code=$?
    [ "$code" -eq 0 ] && return 0
    if [ "$code" -ne 2 ]; then
        mkdir -p "$ws" || return 1
        mkdir -p "$NRVNA_LOG_DIR" || return 1
        log="${NRVNA_LOG_DIR%/}/nrvna-$(basename "$ws").log"
        "$daemon" "$model" "$ws" "$@" >"$log" 2>&1 &
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
