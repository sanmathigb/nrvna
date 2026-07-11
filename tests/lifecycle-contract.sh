#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: lifecycle-contract.sh <engine-bin-dir>}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# status on a workspace with no daemon: exit 1, says not running
mkdir -p "$tmp/ws1"
if "$bin_dir/nrvnad" status "$tmp/ws1"; then
    echo "status on empty workspace should exit 1" >&2; exit 1
fi

# status on a missing workspace: exit 1, no crash
if "$bin_dir/nrvnad" status "$tmp/nope"; then
    echo "status on missing workspace should exit 1" >&2; exit 1
fi

# stale pid/ready/info files are cleaned up when no lock is held
mkdir -p "$tmp/ws2"
echo "999999" > "$tmp/ws2/.nrvnad.pid"
echo "stale" > "$tmp/ws2/.nrvnad.ready"
echo "{}" > "$tmp/ws2/.nrvnad.info"
"$bin_dir/nrvnad" status "$tmp/ws2" && { echo "stale files should read as not running" >&2; exit 1; }
[ ! -f "$tmp/ws2/.nrvnad.pid" ]   || { echo "stale pid not cleaned" >&2; exit 1; }
[ ! -f "$tmp/ws2/.nrvnad.ready" ] || { echo "stale ready not cleaned" >&2; exit 1; }
[ ! -f "$tmp/ws2/.nrvnad.info" ]  || { echo "stale info not cleaned" >&2; exit 1; }

# --json shape when not running
json="$("$bin_dir/nrvnad" status "$tmp/ws1" --json || true)"
case "$json" in
    *'"running":false'*) ;;
    *) echo "status --json missing running:false: $json" >&2; exit 1 ;;
esac

# stop when nothing is running: exit 0, quiet success
mkdir -p "$tmp/ws3"
"$bin_dir/nrvnad" stop "$tmp/ws3" || { echo "stop with no daemon should exit 0" >&2; exit 1; }

# Lifecycle verbs reject unknown, incomplete, and misplaced options.
if "$bin_dir/nrvnad" stop "$tmp/ws3" --timeout >/dev/null 2>&1; then
    echo "stop accepted a missing timeout value" >&2; exit 1
fi
if "$bin_dir/nrvnad" stop "$tmp/ws3" --bogus 5 >/dev/null 2>&1; then
    echo "stop accepted an unknown option" >&2; exit 1
fi
if "$bin_dir/nrvnad" status "$tmp/ws3" --bogus >/dev/null 2>&1; then
    echo "status accepted an unknown option" >&2; exit 1
fi
if "$bin_dir/nrvnad" "$tmp/model.gguf" "$tmp/ws3" --workers 2oops >/dev/null 2>&1; then
    echo "daemon accepted a partially numeric worker count" >&2; exit 1
fi

echo "lifecycle-contract: all checks passed"
