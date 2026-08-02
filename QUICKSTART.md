# Primitive Quick Start

This guide is for builders using `nrvnad`, `wrk`, and `flw` directly. For the
packaged image-search product, start with [apps/imgsrch/README.md](apps/imgsrch/README.md).

## Build

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build && cmake --build build -j4
```

## Get a Model

Use a llama.cpp-compatible instruct GGUF that your current build supports. Put
it in `./models/` or point to it with a full path (`NRVNA_MODELS_DIR` changes
the search path). If you don't have one yet, use the pinned, checksum-verified
model under [Run one job](README.md#run-one-job).

## First Job

Submit work and keep the returned job ID:

```bash
WS=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-quickstart.XXXXXX")
MODEL=./models/smollm2-1.7b.gguf
JOB=$(./build/wrk "$WS" "Reply with exactly: hello")
```

Run the model against the queued work, then retrieve the answer:

```bash
./build/nrvnad "$MODEL" "$WS" --drain
./build/flw "$WS" "$JOB"
```

The answer also lives at `$WS/output/<job-id>/result.txt` as a plain file.
`wrk` did not need a running daemon; the job was already durable on disk.

## Workspace Status

```bash
./build/flw /tmp/ws
./build/flw /tmp/ws --json
```

With no job ID, `flw` shows workspace counts and recent jobs. Exit codes when
reading a job: `0` done, `1` failed, `2` not ready.

## Submit Now, Drain Later, Collect

`wrk` returns a job ID immediately. Jobs remain queued even when no daemon or
submitting process exists. Queue a batch now:

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

# or submit and block for one result in a single pipe
{ echo "Extract every action item:"; cat notes.md; } | ./build/wrk /tmp/ws - | ./build/flw /tmp/ws -w
```

The tag groups jobs; it does not order them, share context, or choose a model.
If a persistent daemon already owns the workspace, wait for just this batch
with `flw /tmp/ws -W --tag "$BATCH"`, then run the separate collection command.
The barrier prints no selected results.

## Other Job Types

Each needs a model built for the job (and mmproj/vocoder files where noted —
`nrvnad` auto-detects them next to the model):

```bash
# Vision — caption or OCR screenshots (vision model + mmproj)
for img in ~/Screenshots/*.png; do
  ./build/wrk /tmp/ws-vision "What is this screenshot about?" --image "$img"
done

# Speech-to-text — transcribe a voice memo (audio-capable model + mmproj)
./build/wrk /tmp/ws-stt --audio memo.mp3 --stt

# Text-to-speech — narrate text (OuteTTS model + vocoder)
cat article-intro.txt | ./build/wrk /tmp/ws-tts - --tts

# Embeddings — vectors for search and similarity (embedding model)
for f in docs/*.md; do cat "$f" | ./build/wrk /tmp/ws-embed - --embed; done
```

One daemon per model, one workspace per daemon. Different workspaces run
side by side.

## Next Steps

- [README.md](README.md)
- [ADVANCED.md](ADVANCED.md) — batch, fan-out, chaining, multi-model patterns
- [CONFIGURATION.md](CONFIGURATION.md) — model and runtime settings
- [ARCHITECTURE.md](ARCHITECTURE.md)
- [apps/imgsrch/README.md](apps/imgsrch/README.md)
