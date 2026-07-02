# Primitive Quick Start

This guide is for builders using `nrvnad`, `wrk`, and `flw` directly. For the
packaged image-search product, start with [apps/imgsrch/README.md](apps/imgsrch/README.md).

## Build

```bash
git clone --recursive https://github.com/sanmathigb/nrvna-ai.git
cd nrvna-ai
cmake -S . -B build && cmake --build build -j4
```

Bring your own GGUF model. Put it in `./models/` or point to it with a full path.

## One Prompt, One Result

```bash
./build/nrvnad models/your-model.gguf /tmp/ws -w 1 &
while [ ! -f /tmp/ws/.nrvnad.pid ]; do sleep 1; done
JOB=$(./build/wrk /tmp/ws "What is the capital of France?")
./build/flw /tmp/ws -w "$JOB"
```

`wrk` prints a job ID immediately. `flw -w` waits for completion, then prints the result.

## Workspace Status

```bash
./build/flw /tmp/ws
./build/flw /tmp/ws --json
```

With no job ID, `flw` shows workspace counts and recent jobs.

## Submit, Walk Away, Collect

`wrk` returns a job ID immediately — you never wait at submit time. Submission
and collection are decoupled; the workspace holds the result until you ask.

```bash
# queue work, then go do something else (you can even close the terminal)
JOB=$(./build/wrk /tmp/ws "Summarize this long document")
# ...later...
./build/flw /tmp/ws -w "$JOB"     # collect when you're ready

# or submit and block for the result in one pipe
./build/wrk /tmp/ws "Quick question" | ./build/flw /tmp/ws -w
```

## Other Job Types

```bash
# Vision
./build/wrk /tmp/ws "What's in this image?" --image photo.jpg

# Speech-to-text
./build/wrk /tmp/ws --audio recording.wav --stt

# Text-to-speech
./build/wrk /tmp/ws "Hello, world" --tts

# Embeddings
./build/wrk /tmp/ws "sentence to embed" --embed
```

## Next Steps

- `README.md`
- `apps/imgsrch/README.md`
- `ARCHITECTURE.md`
- `ADVANCED.md`
