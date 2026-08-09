# nrvna Quickstart

This guide is for builders using `nrvnad`, `wrk`, and `flw` directly. For the
packaged image-search product, start with [apps/imgsrch/README.md](apps/imgsrch/README.md).

## Build

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build && cmake --build build -j4
```

## Get a Model

Use a llama.cpp-compatible GGUF that your build supports. Put it in
`./models/`, or use a full path. `NRVNA_MODELS_DIR` changes the search path.
If you already have a model, use it. Otherwise, use the model below.

```bash
mkdir -p models
curl -fL --continue-at - -o models/smollm2-1.7b.gguf \
  https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/2d4a76a30b4af41ecd395c35725ac11688d4cfe4/smollm2-1.7b-instruct-q4_k_m.gguf

MODEL_SHA256=decd2598bc2c8ed08c19adc3c8fdd461ee19ed5708679d1c54ef54a5a30d4f33
if command -v sha256sum >/dev/null; then
  echo "$MODEL_SHA256  models/smollm2-1.7b.gguf" | sha256sum -c -
else
  echo "$MODEL_SHA256  models/smollm2-1.7b.gguf" | shasum -a 256 -c -
fi
```

This example uses a small model so the first run finishes quickly on older
machines. It is an example, not a limit.

## First Job

Submit work and keep the returned job ID:

```bash
WS=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-quickstart.XXXXXX")
MODEL=./models/smollm2-1.7b.gguf
JOB=$(./build/wrk "$WS" "Reply with exactly: hello")
```

Run the model against the queued work. Then retrieve the answer:

```bash
./build/nrvnad "$MODEL" "$WS" --drain
./build/flw "$WS" "$JOB"
```

The answer also exists as `$WS/output/<job-id>/result.txt`. `wrk` did not need
a running daemon. It made the job durable before the daemon started.

## Workspace Status

```bash
./build/flw /tmp/ws
./build/flw /tmp/ws --json
```

Without a job ID, `flw` shows workspace counts and recent jobs. Job retrieval
uses three exit codes: `0` done, `1` failed or missing, and `2` queued or
running.

## Submit Now, Drain Later, Collect

`wrk` returns a job ID immediately. Jobs stay queued without a daemon or
submitting process. Queue a batch now:

```bash
BATCH="summary-$(date +%s)"
for f in *.md; do
  { echo "Summarize in three bullets:"; cat "$f"; } \
    | ./build/wrk /tmp/ws - --tag "$BATCH"
done

# ...when compute is available, load once and drain the queued work...
./build/nrvnad "$MODEL" /tmp/ws --drain

# ...then collect this batch as NDJSON
./build/flw /tmp/ws --tag "$BATCH" --json

# or submit, drain, and collect one more result
JOB=$({ echo "Extract every action item:"; cat notes.md; } | ./build/wrk /tmp/ws -)
./build/nrvnad "$MODEL" /tmp/ws --drain
./build/flw /tmp/ws "$JOB"
```

The tag groups jobs. It does not order jobs, share context, or choose a model.
If a daemon owns the workspace, wait with `flw /tmp/ws -W --tag "$BATCH"`.
Then run the collection command. The barrier prints no results.

## Other Job Types

Each job type needs a compatible model. Some models also need an `mmproj` or
vocoder. `nrvnad` detects these files beside the model.

```bash
# Vision: caption or OCR screenshots (vision model + mmproj)
for img in ~/Screenshots/*.png; do
  ./build/wrk /tmp/ws-vision "What is this screenshot about?" --image "$img"
done

# Speech-to-text: transcribe a voice memo (audio-capable model + mmproj)
./build/wrk /tmp/ws-stt --audio memo.mp3 --stt

# Text-to-speech: narrate text (OuteTTS model + vocoder)
cat article-intro.txt | ./build/wrk /tmp/ws-tts - --tts

# Embeddings: vectors for search and similarity (embedding model)
for f in docs/*.md; do cat "$f" | ./build/wrk /tmp/ws-embed - --embed; done

# Structured text: constrain one result with JSON Schema
./build/wrk /tmp/ws-text "Extract the requested fields" \
  --json-schema fields.schema.json
```

If the model stops early or emits invalid JSON, the structured job fails and
publishes `error.txt` under `failed/`.

One daemon per model, one workspace per daemon. Different workspaces run
side by side.

## Next Steps

- [README.md](README.md)
- [ADVANCED.md](ADVANCED.md): batch, fan-out, chaining, multi-model patterns
- [CONFIGURATION.md](CONFIGURATION.md): model and runtime settings
- [ARCHITECTURE.md](ARCHITECTURE.md)
- [apps/imgsrch/README.md](apps/imgsrch/README.md)
