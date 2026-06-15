#!/bin/bash

set -euo pipefail

NRVNA_LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NRVNA_PROJECT_DIR="${NRVNA_PROJECT_DIR:-$(cd "$NRVNA_LIB_DIR/.." && pwd)}"
NRVNA_BUILD_DIR="${NRVNA_BUILD_DIR:-$NRVNA_PROJECT_DIR/build}"
NRVNA_DAEMON="${NRVNA_DAEMON_BIN:-}"
NRVNA_WRK="${NRVNA_WRK_BIN:-}"
NRVNA_FLW="${NRVNA_FLW_BIN:-}"

: "${NRVNA_START_TIMEOUT:=120}"
: "${NRVNA_STOP_TIMEOUT:=20}"
: "${NRVNA_POLL_INTERVAL:=0.2}"
: "${NRVNA_LOG_DIR:=/tmp}"

nrvna__err() {
    echo "nrvna-lib: $*" >&2
}

nrvna__note() {
    echo "nrvna-lib: $*" >&2
}

nrvna__find_bin() {
    local explicit="$1"
    local build_name="$2"
    if [ -n "$explicit" ] && [ -x "$explicit" ]; then
        echo "$explicit"
        return 0
    fi
    if [ -x "$NRVNA_BUILD_DIR/$build_name" ]; then
        echo "$NRVNA_BUILD_DIR/$build_name"
        return 0
    fi
    command -v "$build_name" 2>/dev/null || true
}

NRVNA_DAEMON="${NRVNA_DAEMON:-$(nrvna__find_bin "$NRVNA_DAEMON" nrvnad)}"
NRVNA_WRK="${NRVNA_WRK:-$(nrvna__find_bin "$NRVNA_WRK" wrk)}"
NRVNA_FLW="${NRVNA_FLW:-$(nrvna__find_bin "$NRVNA_FLW" flw)}"

nrvna__pid_file() {
    echo "$1/.nrvnad.pid"
}

nrvna__meta_file() {
    echo "$1/.nrvnad.start"
}

nrvna__log_path() {
    local ws="$1"
    local name
    name="$(basename "$ws")"
    echo "${NRVNA_LOG_DIR%/}/nrvna-${name}.log"
}

nrvna__read_pid() {
    local pid_file
    pid_file="$(nrvna__pid_file "$1")"
    [ -f "$pid_file" ] || return 1
    local pid
    pid="$(tr -d '[:space:]' < "$pid_file")"
    [ -n "$pid" ] || return 1
    echo "$pid"
}

nrvna__float_ge() {
    awk -v a="$1" -v b="$2" 'BEGIN { exit !(a >= b) }'
}

nrvna__runtime_config() {
    local model="$1"
    shift
    {
        printf 'model=%s\n' "$model"
        printf 'args='
        printf '%s\037' "$@"
        printf '\n'
        printf 'env.NRVNA_WORKERS=%s\n' "${NRVNA_WORKERS:-4}"
        printf 'env.NRVNA_GPU_LAYERS=%s\n' "${NRVNA_GPU_LAYERS:-0}"
        printf 'env.NRVNA_MAX_CTX=%s\n' "${NRVNA_MAX_CTX:-8192}"
        printf 'env.NRVNA_BATCH=%s\n' "${NRVNA_BATCH:-2048}"
        printf 'env.NRVNA_UBATCH=%s\n' "${NRVNA_UBATCH:-${NRVNA_BATCH:-2048}}"
        printf 'env.NRVNA_PREDICT=%s\n' "${NRVNA_PREDICT:-${NRVNA_N_PREDICT:-2048}}"
        printf 'env.NRVNA_TEMP=%s\n' "${NRVNA_TEMP:-0.8}"
        printf 'env.NRVNA_THINKING=%s\n' "${NRVNA_THINKING:-1}"
        printf 'env.NRVNA_IMAGE_MAX_TOKENS=%s\n' "${NRVNA_IMAGE_MAX_TOKENS:-0}"
        printf 'env.NRVNA_CHAT_TEMPLATE_FILE=%s\n' "${NRVNA_CHAT_TEMPLATE_FILE:-}"
    }
}

nrvna__write_meta() {
    local ws="$1"
    local model="$2"
    shift 2
    nrvna__runtime_config "$model" "$@" > "$(nrvna__meta_file "$ws")"
}

nrvna__same_runtime() {
    local ws="$1"
    local model="$2"
    shift 2
    local meta
    meta="$(nrvna__meta_file "$ws")"
    [ -f "$meta" ] || return 1

    local saved current
    saved="$(cat "$meta")"
    current="$(nrvna__runtime_config "$model" "$@")"
    [ "$saved" = "$current" ]
}

nrvna__pid_is_nrvnad() {
    local pid="$1"
    kill -0 "$pid" 2>/dev/null || return 1
    local comm
    comm="$(ps -p "$pid" -o comm= 2>/dev/null | awk '{print $1}')"
    [ "$(basename "$comm")" = "nrvnad" ]
}

nrvna__lock_is_held() {
    python3 - "$1" <<'PY'
import fcntl, os, sys
path = os.path.join(sys.argv[1], '.nrvnad.lock')
try:
    fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)
except OSError:
    raise SystemExit(1)
try:
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
except BlockingIOError:
    raise SystemExit(0)
else:
    fcntl.flock(fd, fcntl.LOCK_UN)
    raise SystemExit(1)
finally:
    os.close(fd)
PY
}

nrvna_status() {
    local ws="$1"
    local pid
    pid="$(nrvna__read_pid "$ws")" || return 1
    if nrvna__pid_is_nrvnad "$pid" && nrvna__lock_is_held "$ws"; then
        return 0
    fi
    rm -f "$(nrvna__pid_file "$ws")" "$(nrvna__meta_file "$ws")"
    return 1
}

nrvna_start() {
    local model="$1"
    local ws="$2"
    shift 2

    if [ -z "$NRVNA_DAEMON" ] || [ ! -x "$NRVNA_DAEMON" ]; then
        nrvna__err "nrvnad not found (checked NRVNA_DAEMON_BIN, $NRVNA_BUILD_DIR/nrvnad, PATH)"
        return 1
    fi

    mkdir -p "$ws"

    if nrvna_status "$ws"; then
        if nrvna__same_runtime "$ws" "$model" "$@"; then
            nrvna__note "daemon already running for $ws"
            return 0
        fi
        nrvna__note "daemon config changed for $ws; restarting"
        nrvna_stop "$ws"
    fi

    local log launcher_pid elapsed
    log="$(nrvna__log_path "$ws")"
    elapsed="0"

    "$NRVNA_DAEMON" "$model" "$ws" "$@" >"$log" 2>&1 &
    launcher_pid=$!
    nrvna__note "starting daemon (PID $launcher_pid, model $(basename "$model"), ws $ws)"

    while true; do
        if [ -f "$(nrvna__pid_file "$ws")" ]; then
            local pid
            pid="$(nrvna__read_pid "$ws" || true)"
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                nrvna__write_meta "$ws" "$model" "$@"
                nrvna__note "daemon ready (ws $ws)"
                return 0
            fi
        fi

        if ! kill -0 "$launcher_pid" 2>/dev/null; then
            nrvna__err "daemon exited during startup for $ws"
            nrvna__err "log: $log"
            [ -f "$log" ] && tail -20 "$log" >&2
            return 1
        fi

        if nrvna__float_ge "$elapsed" "$NRVNA_START_TIMEOUT"; then
            nrvna__err "timed out waiting for daemon readiness ($NRVNA_START_TIMEOUT s)"
            nrvna__err "log: $log"
            [ -f "$log" ] && tail -20 "$log" >&2
            return 1
        fi

        sleep "$NRVNA_POLL_INTERVAL"
        elapsed="$(awk -v a="$elapsed" -v b="$NRVNA_POLL_INTERVAL" 'BEGIN { printf "%.3f", a + b }')"
    done
}

nrvna_stop() {
    local ws="$1"
    local pid elapsed
    pid="$(nrvna__read_pid "$ws" || true)"
    [ -n "${pid:-}" ] || return 0

    if ! nrvna__pid_is_nrvnad "$pid" || ! nrvna__lock_is_held "$ws"; then
        rm -f "$(nrvna__pid_file "$ws")" "$(nrvna__meta_file "$ws")"
        return 0
    fi

    kill -TERM "$pid" 2>/dev/null || true
    elapsed="0"
    while kill -0 "$pid" 2>/dev/null; do
        if nrvna__float_ge "$elapsed" "$NRVNA_STOP_TIMEOUT"; then
            nrvna__note "worker still busy; forcing stop for $ws"
            kill -TERM "$pid" 2>/dev/null || true
            sleep 2
            if kill -0 "$pid" 2>/dev/null; then
                kill -KILL "$pid" 2>/dev/null || true
            fi
            break
        fi
        sleep "$NRVNA_POLL_INTERVAL"
        elapsed="$(awk -v a="$elapsed" -v b="$NRVNA_POLL_INTERVAL" 'BEGIN { printf "%.3f", a + b }')"
    done

    rm -f "$(nrvna__pid_file "$ws")" "$(nrvna__meta_file "$ws")"
    nrvna__note "daemon stopped (ws $ws)"
}

nrvna_ensure() {
    local model="$1"
    local ws="$2"
    shift 2

    if nrvna_status "$ws"; then
        if nrvna__same_runtime "$ws" "$model" "$@"; then
            return 0
        fi
        nrvna__note "daemon config changed for $ws; restarting"
        nrvna_stop "$ws"
    fi

    nrvna_start "$model" "$ws" "$@"
}
