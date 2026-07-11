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

# A validly-shaped but absent ID is missing, not merely "not ready".
missing_id="00001781482179019395_4090_999999"
missing_rc=0
"$bin_dir/flw" "$tmp" "$missing_id" >/dev/null 2>&1 || missing_rc=$?
[ "$missing_rc" -eq 1 ] || { echo "missing job should exit 1, got $missing_rc" >&2; exit 1; }

if "$bin_dir/flw" "$tmp" "$done_id" "$missing_id" >/dev/null 2>&1; then
    echo "flw accepted multiple job IDs" >&2; exit 1
fi

if "$bin_dir/wrk" "$tmp" "audio" --tts --stt >/dev/null 2>&1; then
    echo "wrk accepted contradictory TTS and STT modes" >&2; exit 1
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

# ── flw sets: --tag selection, NDJSON, --children ────────────────────────
tag_a="00001781482179019400_4090_000004"
tag_b="00001781482179019401_4090_000005"
child="00001781482179019402_4090_000006"
mkdir -p "$tmp/output/$tag_a" "$tmp/output/$tag_b" "$tmp/failed/$child"
printf 'alpha\n' > "$tmp/output/$tag_a/result.txt"
printf '{\n  "submitted_at": "t",\n  "mode": "text",\n  "tags": ["night"]\n}\n' > "$tmp/output/$tag_a/meta.json"
printf 'beta\n' > "$tmp/output/$tag_b/result.txt"
printf '{\n  "submitted_at": "t",\n  "mode": "text",\n  "tags": ["night", "extra"]\n}\n' > "$tmp/output/$tag_b/meta.json"
printf 'boom\n' > "$tmp/failed/$child/error.txt"
printf '{\n  "submitted_at": "t",\n  "mode": "text",\n  "parent": "%s"\n}\n' "$tag_a" > "$tmp/failed/$child/meta.json"

# plain mode: ids one per line, both tagged jobs, nothing else
ids="$("$bin_dir/flw" "$tmp" --tag night)"
[ "$(printf '%s\n' "$ids" | wc -l | tr -d ' ')" = "2" ] || { echo "tag selection wrong count: $ids" >&2; exit 1; }
case "$ids" in *"$tag_a"*"$tag_b"*) ;; *) echo "tag ids wrong/order: $ids" >&2; exit 1 ;; esac

# NDJSON: one object per line, artifact fields present
nd="$("$bin_dir/flw" "$tmp" --tag night --json)"
[ "$(printf '%s\n' "$nd" | wc -l | tr -d ' ')" = "2" ] || { echo "ndjson wrong line count" >&2; exit 1; }
case "$nd" in *'"artifact_kind":"result"'*'"result":"alpha'*) ;; *) echo "ndjson missing artifact/result: $nd" >&2; exit 1 ;; esac

# children selection finds the failed child with its error, and the JSON
# collect exit code reports the set's failure (exit 1)
kids_rc=0
kids="$("$bin_dir/flw" "$tmp" --children "$tag_a" --json)" || kids_rc=$?
case "$kids" in *"$child"*'"status":"failed"'*'"error":"boom'*) ;; *) echo "children selection broken: $kids" >&2; exit 1 ;; esac
[ "$kids_rc" -eq 1 ] || { echo "json collect of failed set should exit 1, got $kids_rc" >&2; exit 1; }

# scoped -W on an already-finished set returns immediately; failed child = exit 1
if "$bin_dir/flw" "$tmp" -W --children "$tag_a"; then
    echo "-W --children with a failed child should exit 1" >&2; exit 1
fi
"$bin_dir/flw" "$tmp" -W --tag night || { echo "-W --tag with all-done set should exit 0" >&2; exit 1; }

echo "primitive-contract: all checks passed"
