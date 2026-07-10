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

# ── Output artifact rule through flw --json ──────────────────────────────
embed_id="00001781482179019398_4090_000002"
mkdir -p "$tmp/output/$embed_id"
printf '{"dim":1,"vector":[0.5]}\n' > "$tmp/output/$embed_id/embedding.json"
json="$("$bin_dir/flw" "$tmp" "$embed_id" --json)"
case "$json" in
    *'"artifact_kind":"embedding"'*) ;;
    *) echo "embedding artifact_kind missing: $json" >&2; exit 1 ;;
esac
case "$json" in
    *'"artifact_path":"'*embedding.json'"'*) ;;
    *) echo "embedding artifact_path missing: $json" >&2; exit 1 ;;
esac

# Priority: result.txt beats transcript.txt in the same job dir
both_id="00001781482179019399_4090_000003"
mkdir -p "$tmp/output/$both_id"
printf 'the result\n' > "$tmp/output/$both_id/result.txt"
printf 'the transcript\n' > "$tmp/output/$both_id/transcript.txt"
json="$("$bin_dir/flw" "$tmp" "$both_id" --json)"
case "$json" in
    *'"artifact_kind":"result"'*) ;;
    *) echo "artifact priority broken: $json" >&2; exit 1 ;;
esac

echo "primitive-contract: all checks passed"
