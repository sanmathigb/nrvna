#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: primitive-contract.sh <engine-bin-dir>}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

done_id="00001781482179019396_4090_000000"
mkdir -p "$tmp/output/$done_id"
printf 'unrelated\n' > "$tmp/output/$done_id/result.txt"

if printf '' | "$bin_dir/flw" "$tmp" -w >/dev/null 2>&1; then
    echo "flw -w accepted empty piped input" >&2
    exit 1
fi

bad_id="00001781482179019397_4090_000001"
mkdir -p "$tmp/input/ready/$bad_id"
counts="$("$bin_dir/flw" "$tmp" --json)"
case "$counts" in
    *'"queued":0'*) ;;
    *)
        echo "malformed ready job counted as queued: $counts" >&2
        exit 1
        ;;
esac
