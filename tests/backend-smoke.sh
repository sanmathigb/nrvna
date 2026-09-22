#!/usr/bin/env bash
# Opt-in real-model check. No downloads; retain artifacts for inspection.
set -euo pipefail
bin_dir="${1:?usage: backend-smoke.sh <bin-dir> <text-model> <embedding-model> [vision-model mmproj]}"
text_model="${2:?text model required}"
embedding_model="${3:?embedding model required}"
smoke_dir="$(mktemp -d)"
echo "Smoke artifacts: $smoke_dir"
export NRVNA_GPU_LAYERS=0 NRVNA_PREDICT=32 NRVNA_MAX_CTX=2048
export NRVNA_BATCH=512 NRVNA_UBATCH=512 NRVNA_TEMP=0 NRVNA_THINKING=0

printf '%s\n' '{"type":"object","properties":{"answer":{"const":"ready"}},"required":["answer"],"additionalProperties":false}' > "$smoke_dir/schema.json"
text_id=$("$bin_dir/wrk" "$smoke_dir/text" "Reply with exactly: ready")
json_id=$("$bin_dir/wrk" "$smoke_dir/text" 'Return {"answer":"ready"}.' --json-schema "$smoke_dir/schema.json")
"$bin_dir/nrvnad" status "$smoke_dir/text" --json || test "$?" -eq 1
"$bin_dir/nrvnad" "$text_model" "$smoke_dir/text" --workers 1 --drain > "$smoke_dir/text.log" 2>&1
"$bin_dir/flw" "$smoke_dir/text" "$text_id" --json > "$smoke_dir/text.json"
"$bin_dir/flw" "$smoke_dir/text" "$json_id" > "$smoke_dir/answer.json"
python3 -c 'import json,sys; assert json.load(open(sys.argv[1])) == {"answer":"ready"}' "$smoke_dir/answer.json"

embed_id=$("$bin_dir/wrk" "$smoke_dir/embed" "search_query: local inference" --embed)
"$bin_dir/nrvnad" status "$smoke_dir/embed" --json || test "$?" -eq 1
"$bin_dir/nrvnad" "$embedding_model" "$smoke_dir/embed" --workers 1 --drain > "$smoke_dir/embed.log" 2>&1
"$bin_dir/flw" "$smoke_dir/embed" "$embed_id" --json > "$smoke_dir/embed.json"
python3 - "$smoke_dir/embed/output/$embed_id/embedding.json" <<'PY'
import json, math, sys
v = json.load(open(sys.argv[1]))
while isinstance(v, dict):
    v = v.get("vector", v.get("embedding"))
assert isinstance(v, list) and v and all(math.isfinite(x) for x in v)
assert abs(sum(x*x for x in v) - 1) < 0.01
print("embedding dimensions:", len(v))
PY

if [ "$#" -ge 5 ]; then
    # Synthetic input tests the media API without expensive screenshot tiling.
    python3 - "$smoke_dir/red.png" <<'PY'
import struct, sys, zlib
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
with open(sys.argv[1], 'wb') as f:
    f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress((b'\0' + b'\xff\0\0' * 2) * 2)) + chunk(b'IEND', b''))
PY
    vision_id=$("$bin_dir/wrk" "$smoke_dir/vision" "Name the dominant color." --image "$smoke_dir/red.png")
    "$bin_dir/nrvnad" status "$smoke_dir/vision" --json || test "$?" -eq 1
    "$bin_dir/nrvnad" "$4" "$smoke_dir/vision" --mmproj "$5" --workers 1 --drain > "$smoke_dir/vision.log" 2>&1
    "$bin_dir/flw" "$smoke_dir/vision" "$vision_id"
    test -s "$smoke_dir/vision/output/$vision_id/result.txt"
fi
echo "Backend smoke checks passed."
