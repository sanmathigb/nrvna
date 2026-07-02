# nrvna-ai

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Local async inference primitives. `nrvnad` runs models, `wrk` submits jobs,
and `flw` collects results. The filesystem is the queue.

## Primitive Quick Start

Build the primitives from source:

```bash
git clone --recursive https://github.com/sanmathigb/nrvna-ai.git
cd nrvna-ai
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Start a daemon with any GGUF model, submit work, and collect the result:

```bash
./build/nrvnad models/your-model.gguf /tmp/ws -w 1 &
while [ ! -f /tmp/ws/.nrvnad.pid ]; do sleep 1; done

JOB=$(./build/wrk /tmp/ws "Explain the CAP theorem in two sentences")
./build/flw /tmp/ws -w "$JOB"
```

## See It Work

Submit multiple jobs, inspect workspace progress, and collect results:

```bash
# wrk returns a job ID immediately — fire off work and walk away
JOB1=$(./build/wrk /tmp/ws "Explain Raft in two sentences")
JOB2=$(./build/wrk /tmp/ws "Summarize the CAP theorem in one sentence")

# check progress any time; the queue is just the filesystem
./build/flw /tmp/ws
./build/flw /tmp/ws --json

# collect each result whenever you come back
./build/flw /tmp/ws -w "$JOB1"
./build/flw /tmp/ws -w "$JOB2"

# or submit and block for one result in a single pipe
./build/wrk /tmp/ws "Quick question" | ./build/flw /tmp/ws -w
```

The substrate is the point: submission and collection are decoupled. Durable
jobs, inspectable state, predictable retrieval — through the same three binaries.

## Three Primitives

| Tool | What it does |
|------|-------------|
| `nrvnad` | Load a model, watch a workspace, process jobs |
| `wrk` | Submit work, get back a job ID immediately |
| `flw` | Inspect a workspace, or collect a result by job ID |

That's the entire API. Everything else is composition.

## Job Types

```bash
# Text
wrk ./ws "Summarize this document"

# Vision — caption, describe, OCR
wrk ./ws "What's in this image?" --image photo.jpg

# Speech-to-text — transcribe audio
wrk ./ws --audio recording.wav --stt

# Text-to-speech — synthesize audio
wrk ./ws "Hello, world" --tts

# Embeddings — vectors for search/similarity
wrk ./ws "sentence to embed" --embed
```

Multimodal support depends on the model files you provide: GGUF model,
matching mmproj for vision or speech models, and vocoder where required.

## How It Works

Jobs are directories. State is location. Transitions are atomic renames.

```text
workspace/
├── input/ready/    queued jobs
├── processing/     jobs being worked
├── output/         results: result.txt, embedding.json, audio.wav
└── failed/         errors: error.txt
```

No database. No message broker. No runtime dependency beyond the binaries and
models. Every job is fresh: bounded context, no session drift, predictable
output.

## Workflows

Workflows sit above the core release surface. Common patterns are:

- multi-job text processing
- multimodal ingestion
- chunked TTS or transcription
- map-reduce over large documents
- local apps that submit work and pick up files later

## First-Party Apps

These are real apps built on the primitives, not alternate inference paths.

- [`apps/imgsrch`](apps/imgsrch/README.md): local image search. A Go CLI that
  packages `nrvnad`, `wrk`, and `flw`, manages pinned models, indexes images,
  and searches them from one command.
- [`apps/bckbrnr`](apps/bckbrnr/README.md): local prompt work from the menu bar.
  A macOS app that bundles the primitives, starts a text utility, and writes
  answers back as files.

## Why

nrvna is compelling when the job is bigger than one prompt and smaller than a
whole framework.

- **Not a chat app**: async jobs, not conversations
- **Not an agent framework**: primitives you build on
- **Not a model runtime**: llama.cpp runs the model; nrvna adds jobs,
  workspaces, and composition

## Platform

The primitives build on macOS and Linux. Release archives currently target
Apple Silicon macOS, Intel macOS, and Linux x86-64 for the first-party CLI app.
Primitive users can opt into supported llama.cpp GPU backends with
`NRVNA_GPU_LAYERS`.

## Primitive Build Requirements

- macOS or Linux
- CMake 3.16+ and a C++17 compiler
- GGUF models you provide yourself

Packaged apps may bundle the primitive binaries, but model licensing and model
downloads remain app-specific.

## License

MIT
