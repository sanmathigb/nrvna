# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Durable local inference primitives for GGUF models.

Submit work now. Run the model when compute is available. Read ordinary files
later.

```text
wrk  ->  workspace  <->  nrvnad + model
            |
           flw
```

nrvna is built on [llama.cpp](https://github.com/ggml-org/llama.cpp), which
loads and runs the models. nrvna adds durable jobs, workspaces, process
lifecycle, and file-based composition.

## Quick start

Build the three binaries:

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Download a small llama.cpp-compatible instruct model (about 1 GB,
Apache-2.0) from an immutable Hugging Face revision, then verify it:

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

Submit two jobs before any model process exists:

```bash
BIN=./build
WS=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-demo.XXXXXX")
MODEL=./models/smollm2-1.7b.gguf

JOB1=$("$BIN/wrk" "$WS" "Reply with exactly: first" --tag demo)
JOB2=$("$BIN/wrk" "$WS" "Reply with exactly: second" --tag demo)
"$BIN/flw" "$WS"
```

At this point both jobs are queued on disk. Load the model, drain the queue,
and exit:

```bash
"$BIN/nrvnad" "$MODEL" "$WS" --workers 2 --drain
"$BIN/flw" "$WS" --tag demo --json
```

Results remain under `$WS/output/<job-id>/`. Nothing needs to stay running.

## Three primitives

| Command | Contract |
| --- | --- |
| `wrk` | Publish an independent job and print its ID |
| `nrvnad` | Load one model and process a workspace |
| `flw` | Inspect status or read terminal results |

The CLI is the API. Scripts and agents compose it with stdin, files, JSON, and
exit codes.

## Why

Many local model tools expose a live request to a resident server. That is
useful for chat and interactive completion.

nrvna is for work that should outlive the process waiting for it:

- queue a batch before loading a model;
- let specialized models take turns on constrained hardware;
- survive terminal, caller, or daemon restarts;
- inspect every input, state transition, result, and failure;
- collect the work from another script or agent session.

The model process is temporary. The workspace is the durable record.

## Files are the state

```text
workspace/
├── input/ready/   queued
├── processing/    claimed
├── output/        result.txt, embedding.json, transcript.txt, audio.wav
└── failed/        error.txt
```

Submission, claims, and terminal publication use atomic filesystem renames.
After a crash, the next daemon recovers abandoned jobs from `processing/`.

Execution is at least once. Failures are preserved and terminal. Retry policy
belongs to the caller.

## Fresh context

Every `wrk` submission is a new model context. Context does not carry between
jobs.

`--parent` records lineage only. It does not copy context, wait for another
job, or impose execution order. Put any prior evidence needed by a new job
into its prompt explicitly.

The workspace remembers; the model does not.

## Job types

| Work | Submit with | Primary artifact |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Embedding | `--embed` | `embedding.json` |
| Vision | `--image` | `result.txt` |
| Speech to text | `--audio ... --stt` | `transcript.txt` |
| Text to speech | `--tts` | `audio.wav` |

Vision and speech models may require an `mmproj`; TTS may require a vocoder.
Place matching files beside the model and `nrvnad` will auto-detect them.

## For agents

Give a coding agent this sentence:

```text
Read https://raw.githubusercontent.com/sanmathigb/nrvna/main/AGENTS.md.
Explain nrvna's job, context, drain, and failure contracts before using it.
Use an isolated workspace for experiments and do not download models without
asking me first.
```

`AGENTS.md` documents stdout, JSON, exit codes, tags, lineage, daemon
lifecycle, artifacts, and recovery behavior.

## Built with nrvna

- [imgsrch](apps/imgsrch/README.md) searches local screenshots by visible
  words and meaning.
- [bckbrnr](apps/bckbrnr/README.md) runs local prompt work from the macOS menu
  bar and writes answers back as files.

These applications use `nrvnad`, `wrk`, and `flw` directly. They do not carry
an alternate inference path.

## Boundaries

nrvna is not a chat API, agent framework, model router, semantic index, or
distributed queue.

It does not assemble parent context, execute DAGs, choose models, parse
documents, validate model output, or retry failures automatically.

[llama.cpp](https://github.com/ggml-org/llama.cpp) owns model inference. The
calling application owns orchestration and product behavior. nrvna owns the
durable local work between them.

## Documentation

- [QUICKSTART.md](QUICKSTART.md): guided use of the primitives
- [ADVANCED.md](ADVANCED.md): batches, tags, chaining, and multiple models
- [ARCHITECTURE.md](ARCHITECTURE.md): ownership and state transitions
- [AGENTS.md](AGENTS.md): machine and agent contract

## Support

nrvna currently builds on macOS and Linux with CMake 3.16+ and a C++17
compiler. CPU inference is the default. Supported llama.cpp GPU backends can
be enabled with `NRVNA_GPU_LAYERS`.

This is an experimental developer preview. The filesystem and lifecycle
contracts are tested, but the project has not yet earned production claims.

MIT licensed. Model licenses remain model-specific.
