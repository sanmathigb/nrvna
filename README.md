# nrvna-ai

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Local inference as durable jobs. One product, three primitives. Filesystem is
the queue.

This repository has two surfaces:

- **imgsrch** is the user-facing product: a native local image-search tool that
  manages its llama.cpp backend and models behind one command.
- **nrvna primitives** are the builder-facing substrate: `nrvnad`, `wrk`, and
  `flw` for composing other durable local inference tools.

## imgsrch

The release archive contains:

```text
imgsrch-<platform>/
├── imgsrch
└── bin/
    ├── nrvnad
    ├── wrk
    └── flw
```

No compiler, Node.js, Python, or separate llama.cpp installation is required.
Models are downloaded on first setup:

```bash
./imgsrch setup
./imgsrch init my-images
./imgsrch add my-images ~/Pictures/*.png
./imgsrch index my-images
./imgsrch status my-images
./imgsrch search my-images "diagram explaining KV cache"
```

`setup` downloads about 3.4 GB of pinned caption, OCR, and embedding models.
Indexing runs in local background workers. See [imgsrch.md](imgsrch.md) for the
complete product workflow.

## Primitive Quick Start

Builders can compile the primitives from source:

```bash
# Build
git clone --recursive https://github.com/sanmathigb/nrvna-ai.git
cd nrvna-ai && cmake -S . -B build && cmake --build build -j4

# Start a daemon with any GGUF model
./build/nrvnad models/your-model.gguf /tmp/ws -w 1 &
while [ ! -f /tmp/ws/.nrvnad.pid ]; do sleep 1; done
JOB=$(./build/wrk /tmp/ws "Explain the CAP theorem in two sentences")
./build/flw /tmp/ws -w "$JOB"
```

## See It Work

Submit multiple jobs, inspect workspace progress, and collect results:

```bash
# Queue a few jobs
JOB1=$(./build/wrk /tmp/ws "Explain Raft in two sentences")
JOB2=$(./build/wrk /tmp/ws "Summarize the CAP theorem in one sentence")

# Inspect workspace status
./build/flw /tmp/ws
./build/flw /tmp/ws --json

# Retrieve results
./build/flw /tmp/ws -w "$JOB1"
./build/flw /tmp/ws -w "$JOB2"
```

The substrate is the point: durable jobs, inspectable state, and predictable retrieval through the same three binaries.

## Three Primitives

| Tool | What it does |
|------|-------------|
| `nrvnad` | Load a model, watch a workspace, process jobs |
| `wrk` | Submit a prompt, get back a job ID |
| `flw` | Retrieve a result by job ID |

That's the entire API. Everything else is composition.

```bash
# Start a daemon
./build/nrvnad models/Qwen2.5-7B-Instruct-Q4_K_M.gguf ./ws -w 1 &
while [ ! -f ./ws/.nrvnad.pid ]; do sleep 1; done

# Submit work
JOB=$(./build/wrk ./ws "Explain the CAP theorem in two sentences")

# Collect result
./build/flw ./ws -w $JOB
```

## Job Types

```bash
# Text (default)
wrk ./ws "Summarize this document"

# Vision — caption, describe, OCR (mmproj auto-detected)
wrk ./ws "What's in this image?" --image photo.jpg

# Embeddings — vectors for search/similarity
wrk ./ws "sentence to embed" --embed

# Text-to-speech — audio output (vocoder auto-detected)
wrk ./ws "Hello, world" --tts
```

## How It Works

Jobs are directories. State is location. Transitions are atomic renames.

```
workspace/
├── input/ready/    ← queued jobs
├── processing/     ← jobs being worked
├── output/         ← results (result.txt, embedding.json, or audio.wav)
└── failed/         ← errors (error.txt)
```

No database. No message broker. No runtime dependencies. Every job is fresh — bounded context, no session drift, predictable output.

## Workflows

Workflows sit above the core release surface. Common patterns are:

- multi-job text processing
- multimodal ingestion
- chunked TTS or transcription
- map-reduce over large documents

## Why

nrvna is compelling when the job is bigger than one prompt and smaller than a whole framework.

- **Not a chat app** — async jobs, not conversations
- **Not an agent framework** — primitives you build on
- **Not a model runtime** — that's llama.cpp underneath. nrvna adds jobs, workspaces, and composition.

## Platform

Release archives target Apple Silicon macOS, Intel macOS, and Linux x86-64.
The `imgsrch` MVP uses CPU inference by default for predictable cross-platform
behavior. Primitive users can opt into supported llama.cpp GPU backends with
`NRVNA_GPU_LAYERS`.

## Primitive Build Requirements

- macOS or Linux
- CMake 3.16+ and a C++17 compiler
- GGUF models you provide yourself ([HuggingFace](https://huggingface.co/models?search=gguf))

These build requirements apply to primitive development, not to packaged
`imgsrch` users.

## License

MIT
