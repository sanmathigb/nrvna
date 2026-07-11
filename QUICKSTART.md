# Primitive Quick Start

This guide is for builders using `nrvnad`, `wrk`, and `flw` directly. For the
packaged image-search product, start with [apps/imgsrch/README.md](apps/imgsrch/README.md).

## Build

```bash
git clone --recursive https://github.com/sanmathigb/nrvna-ai.git
cd nrvna-ai
cmake -S . -B build && cmake --build build -j4
```

## Get a Model

Most llama.cpp-supported GGUF text models work. Put one in `./models/` or point to it with a full path
(`NRVNA_MODELS_DIR` changes the search path). If you don't have one yet:

```bash
mkdir -p models
curl -L -o models/smollm2-1.7b.gguf \
  https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/main/smollm2-1.7b-instruct-q4_k_m.gguf
```

## First Job

Submit before starting anything. This is not a trick — the queue is a folder,
so a daemon doesn't need to exist yet:

```bash
JOB=$({ echo "Summarize this document in five bullets:"; cat ARCHITECTURE.md; } | ./build/wrk /tmp/ws -)
./build/flw /tmp/ws        # shows: queued: 1
```

The prompt is whatever arrives on stdin — compose it however you like. The
`{ echo "instruction"; cat file; }` idiom is the standard way to pair an
instruction with a document.

Now bring a worker to the queue and collect:

```bash
./build/nrvnad models/smollm2-1.7b.gguf /tmp/ws -w 1 &
./build/flw /tmp/ws -w "$JOB"     # blocks until done, prints the result
```

The answer also lives at `/tmp/ws/output/<job-id>/result.txt` — a plain file,
still there tomorrow.

## Workspace Status

```bash
./build/flw /tmp/ws
./build/flw /tmp/ws --json
```

With no job ID, `flw` shows workspace counts and recent jobs. Exit codes when
reading a job: `0` done, `1` failed, `2` not ready.

## Submit, Walk Away, Collect

`wrk` returns a job ID immediately — you never wait at submit time. Queue a
batch, close the terminal, come back whenever:

```bash
for f in *.md; do
  { echo "Summarize in three bullets:"; cat "$f"; } | ./build/wrk /tmp/ws -
done

# ...later, from any terminal...
./build/flw /tmp/ws                    # how's it going?
./build/flw /tmp/ws -W                 # or block until the workspace is idle
cat /tmp/ws/output/*/result.txt        # collect everything

# or submit and block for one result in a single pipe
{ echo "Extract every action item:"; cat notes.md; } | ./build/wrk /tmp/ws - | ./build/flw /tmp/ws -w
```

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
- [ARCHITECTURE.md](ARCHITECTURE.md)
- [apps/imgsrch/README.md](apps/imgsrch/README.md)
