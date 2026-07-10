# nrvna-ai

[![CI](https://github.com/sanmathigb/nrvna-ai/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna-ai/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Local async inference primitives. `nrvnad` runs models, `wrk` submits jobs,
and `flw` collects results. The filesystem is the queue.

Models get wrk. Humans get back flw.

## Quick Start

Build the primitives from source:

```bash
git clone --recursive https://github.com/sanmathigb/nrvna-ai.git
cd nrvna-ai
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Grab a small model (~1 GB, Apache-2.0) — or use any GGUF you already have:

```bash
mkdir -p models
curl -L -o models/smollm2-1.7b.gguf \
  https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/main/smollm2-1.7b-instruct-q4_k_m.gguf
```

Submit real work — before any daemon is running. The queue is just a folder:

```bash
JOB=$({ echo "Summarize this document in five bullets:"; cat ARCHITECTURE.md; } | ./build/wrk /tmp/ws -)
./build/flw /tmp/ws
```

```text
queued:     1
running:    0
done:       0
failed:     0

recent:
  [queued]        00001783014028941539_78592_000000
```

Bring a worker to the queue, then collect the result:

```bash
./build/nrvnad models/smollm2-1.7b.gguf /tmp/ws -w 1 &
./build/flw /tmp/ws -w "$JOB"
```

```text
- Jobs are directories that move through input/ready, processing, and output
  via atomic renames, so state is always visible and crash-safe.
- A scanner thread finds ready jobs and a worker pool runs inference...
```

The result is a plain file at `/tmp/ws/output/<job-id>/result.txt`. It does
not expire. It is still there tomorrow.

## Submit, Walk Away, Collect

`wrk` returns a job ID in milliseconds; the work happens in the background.
Queue everything, then leave:

```bash
for f in *.md; do
  { echo "Summarize in three bullets:"; cat "$f"; } | ./build/wrk /tmp/ws -
done

# close the terminal. sleep the laptop. the queue is on disk.

./build/flw /tmp/ws                  # later: counts and recent jobs
./build/flw /tmp/ws --json           # same, for scripts
cat /tmp/ws/output/*/result.txt      # every answer, as plain files
```

Submission and collection are decoupled. Durable jobs, inspectable state,
predictable retrieval — through the same three binaries.

## From Scripts and Agents

The whole batch idiom is three lines — no daemon to manage:

```bash
for f in notes/*.md; do cat "$f" | wrk ./ws - --tag batch1; done
nrvnad model.gguf ./ws --drain          # process everything queued, then exit
flw ./ws --tag batch1 --json            # NDJSON: one job per line, results inline
```

`--drain` exits 0 when the queue is quiet (1 if any job failed this run) and
leaves nothing running. For long-lived daemons instead:

```bash
nrvnad status ./ws      # exit 0 ready, 2 starting, 1 not running (--json for details)
nrvnad stop ./ws        # graceful shutdown
flw ./ws -W --tag mine  # block until YOUR jobs finish, not the whole workspace
```

Exit codes, JSON, and files — nothing else to learn. Anything with a shell,
including a coding agent, can drive all of it.

## Three Primitives

| Tool | What it does |
|------|-------------|
| `nrvnad` | Load a model, watch a workspace, process jobs |
| `wrk` | Submit work, get back a job ID immediately |
| `flw` | Inspect a workspace, or collect a result by job ID |

That's the entire API. Everything else is composition.

## Job Types

```bash
# Text — transform documents
{ echo "Extract every action item:"; cat meeting-notes.txt; } | wrk ./ws -

# Vision — caption or OCR a folder of screenshots
for img in ~/Screenshots/*.png; do
  wrk ./ws "What is this screenshot about?" --image "$img"
done

# Speech-to-text — transcribe a voice memo
wrk ./ws --audio memo.mp3 --stt

# Text-to-speech — narrate an article
cat article-intro.txt | wrk ./ws - --tts

# Embeddings — index a folder for semantic search
for f in docs/*.md; do cat "$f" | wrk ./ws - --embed; done
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

## Built with nrvna

The primitives are meant to carry a family of small, sharp apps — the way
llama.cpp carries the tools built on top of it. Each app bundles the
primitives and adds no alternate inference path: the app is the surface,
`nrvnad`/`wrk`/`flw` are the engine.

- [`apps/imgsrch`](apps/imgsrch/README.md): local image search. A Go CLI that
  packages the primitives, manages pinned models, indexes images, and
  searches them with hybrid BM25 + embedding retrieval — from one command.
- [`apps/bckbrnr`](apps/bckbrnr/README.md): local prompt work from the menu bar.
  A macOS app that starts a text utility and writes answers back as files.

This list is meant to grow. If you build something on nrvna, open an issue —
"built with nrvna" is the credit line.

## Why

nrvna is compelling when the job is bigger than one prompt and smaller than a
whole framework.

- **Not a chat app**: async jobs, not conversations
- **Not an agent framework**: primitives you build on
- **Not a model runtime**: llama.cpp runs the model; nrvna adds jobs,
  workspaces, and composition

## Learn More

- [QUICKSTART.md](QUICKSTART.md) — the guided path for builders
- [ADVANCED.md](ADVANCED.md) — batch, fan-out, chaining, multi-model patterns
- [ARCHITECTURE.md](ARCHITECTURE.md) — how the pieces fit together
- [docs/how-it-works.md](docs/how-it-works.md) — the philosophy

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
