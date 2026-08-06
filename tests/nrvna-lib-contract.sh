#!/usr/bin/env bash
set -euo pipefail

repo="${1:?usage: nrvna-lib-contract.sh <repository>}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/bin"
cat > "$tmp/bin/nrvnad" <<'FAKE'
#!/usr/bin/env bash
set -u

case "${1:-}" in
    status)
        ws="$2"
        [ -f "$ws/ready" ] && exit 0
        [ -f "$ws/starting" ] && exit 2
        exit 1
        ;;
    stop)
        ws="$2"
        touch "$ws/stop"
        rm -f "$ws/ready" "$ws/starting"
        exit 0
        ;;
esac

model="$1"
ws="$2"
mkdir -p "$ws"
printf '%s\n' "$model" >> "$ws/launches"
if [ "$model" = fail ]; then
    echo "model failed to load" >&2
    exit 7
fi
touch "$ws/starting"
sleep 0.1
mv "$ws/starting" "$ws/ready"
while [ ! -f "$ws/stop" ]; do sleep 0.1; done
rm -f "$ws/ready"
FAKE
chmod +x "$tmp/bin/nrvnad"

# Sourcing the helper must not enable strict shell options in its caller.
shell_check="$(bash -c '
  set +e +u
  set +o pipefail
  source "$1/scripts/nrvna-lib.sh"
  false
  printf survived
' _ "$repo")"
[ "$shell_check" = survived ] || {
    echo "nrvna-lib changed caller shell options" >&2
    exit 1
}

export NRVNA_DAEMON_BIN="$tmp/bin/nrvnad"
export NRVNA_LOG_DIR="$tmp/logs"
export NRVNA_START_TIMEOUT=3
mkdir -p "$NRVNA_LOG_DIR"
source "$repo/scripts/nrvna-lib.sh"

# Resolve the daemon from an override, a build directory, or PATH.
[ "$(nrvna__bin)" = "$tmp/bin/nrvnad" ]
unset NRVNA_DAEMON_BIN
NRVNA_BUILD_DIR="$tmp/bin"
[ "$(nrvna__bin)" = "$tmp/bin/nrvnad" ]
NRVNA_BUILD_DIR="$tmp/missing"
PATH="$tmp/bin:$PATH"
[ "$(nrvna__bin)" = "$tmp/bin/nrvnad" ]
NRVNA_DAEMON_BIN="$tmp/bin/nrvnad"

# Reject invalid helper input without relying on strict caller options.
if nrvna_start 2>"$tmp/usage.err"; then
    echo "nrvna_start accepted missing arguments" >&2
    exit 1
fi
grep -q 'usage: nrvna_start' "$tmp/usage.err"
NRVNA_START_TIMEOUT=invalid
if nrvna_start model.gguf "$tmp/invalid" 2>"$tmp/invalid.err"; then
    echo "nrvna_start accepted an invalid timeout" >&2
    exit 1
fi
grep -q 'must be a nonnegative integer' "$tmp/invalid.err"
NRVNA_START_TIMEOUT=3

# Start an absent daemon, wait for readiness, then stop it.
ws="$tmp/start"
nrvna_start model.gguf "$ws"
nrvna_status "$ws"
[ "$(wc -l < "$ws/launches")" -eq 1 ]
nrvna_stop "$ws"

# Use a daemon that another process is already starting.
ws="$tmp/adopt"
mkdir -p "$ws"
touch "$ws/starting"
(sleep 1; mv "$ws/starting" "$ws/ready") &
starter=$!
nrvna_start model.gguf "$ws"
wait "$starter"
[ ! -f "$ws/launches" ] || {
    echo "nrvna_start launched a second daemon" >&2
    exit 1
}

# Report startup failure and include the daemon log.
ws="$tmp/fail"
if nrvna_start fail "$ws" 2>"$tmp/failure.err"; then
    echo "nrvna_start accepted a failed daemon" >&2
    exit 1
fi
grep -q 'daemon exited during startup' "$tmp/failure.err"
grep -q 'model failed to load' "$tmp/failure.err"

# Stop waiting after the configured timeout.
ws="$tmp/timeout"
mkdir -p "$ws"
touch "$ws/starting"
NRVNA_START_TIMEOUT=1
if nrvna_start model.gguf "$ws" 2>"$tmp/timeout.err"; then
    echo "nrvna_start ignored its timeout" >&2
    exit 1
fi
grep -q 'startup timeout' "$tmp/timeout.err"

echo "nrvna-lib-contract: all checks passed"
