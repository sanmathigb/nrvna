# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Durable local inference primitives. `nrvnad` runs models, `wrk` submits jobs,
and `flw` collects results. The filesystem is the queue.

Models get wrk. Humans get back flw.

Every `wrk` submission is an independent durable task. Context does not carry
between jobs, and `--parent` records lineage only. The workspace remembers;
the model does not.

## Quick Start

Build the primitives from source:

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Grab a supported llama.cpp-compatible instruct GGUF (~1 GB, Apache-2.0):

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
  via atomic renames, so state is always visible and survives process crashes.
- A scanner thread finds ready jobs and a worker pool runs inference...
```

The result is a plain file at `/tmp/ws/output/<job-id>/result.txt`. It does
not expire. It is still there tomorrow.

## Submit, Walk Away, Collect

`wrk` returns a job ID in milliseconds. Execution is independent: a running
daemon processes the queue, and without one the jobs wait durably. Queue
everything, then leave:

```bash
for f in *.md; do
  { echo "Summarize in three bullets:"; cat "$f"; } | ./build/wrk /tmp/ws -
done

# close the submitting terminal. if the daemon stops, the queue stays on disk.

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
leaves nothing running that it started. If a daemon already owns the
workspace, drain waits for it to finish the queue instead — same
postcondition, and your daemon stays up. For long-lived daemons:

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

- [`apps/imgsrch`](apps/imgsrch/README.md): local screenshot and image search
  by visible words or meaning. It gives humans ranked originals and gives
  agents a small, grounded candidate set instead of an entire image library.
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
- [CONTEXT.md](CONTEXT.md) — the domain language and contracts
- [AGENTS.md](AGENTS.md) — the operational contract for agents

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
