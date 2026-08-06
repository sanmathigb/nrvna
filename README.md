# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/llama.cpp-00fa7cb-orange.svg)](https://github.com/ggml-org/llama.cpp/commit/00fa7cb284cbf133fc426733bd64238a3588a33e)

Unix-like primitives for durable local inference.

**No always-on server.** Submit work before any model process is running. No
HTTP server or message broker is required. Run the model when compute is
available. Read ordinary files later.

**Experimental developer preview.** Tests cover the filesystem and lifecycle
contracts. nrvna does not claim production readiness.

```text
wrk  ->  workspace  <->  nrvnad + model
            |
           flw
```

The workspace remembers. The model does not.

[llama.cpp](https://github.com/ggml-org/llama.cpp) loads and runs the GGUF
models. nrvna adds durable jobs, workspaces, process lifecycle, and file-based
composition.

## Run one job

Build the three binaries:

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Use any compatible instruct GGUF:

```bash
MODEL=/path/to/model.gguf
WS=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-demo.XXXXXX")

JOB=$(./build/wrk "$WS" "Reply with exactly: first")
./build/flw "$WS"                       # queued: 1
./build/nrvnad "$MODEL" "$WS" --drain
./build/flw "$WS" "$JOB"
```

```text
first
```

`wrk` creates the workspace and returns immediately. `nrvnad --drain` loads
the model and processes the queued work. It exits when it observes an idle
queue. The result remains under `$WS/output/$JOB/`. No process must stay open.

<details>
<summary>Need a small verified model?</summary>

This command downloads SmolLM2 1.7B Q4_K_M from an immutable Hugging Face
revision. The model is about 1 GB and uses the Apache-2.0 license. The command
also verifies the checksum.

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

</details>

## Three primitives

| Command | Contract |
| --- | --- |
| `wrk` | Publish one independent job and print its ID |
| `nrvnad` | Load one model and process one workspace |
| `flw` | Inspect status or read terminal results |

The commands and published job artifacts form the interface. Humans, scripts,
applications, and agents compose them with stdin, files, JSON, and exit codes.

## One model, one workspace, one drain

One daemon loads one model and owns one workspace. `wrk` can submit jobs while
no daemon is running. `nrvnad --drain` processes that workspace and then exits.

Use separate workspaces for different model roles. Drain them in sequence when
their models cannot share memory.

Shell applications can source [`scripts/nrvna-lib.sh`](scripts/nrvna-lib.sh).
`nrvna_start` starts a daemon or uses one that is already starting. It waits
for readiness and reports startup failures from the daemon log.

## Why

I built nrvna on a 2017 Intel MacBook while caring for two young children. I
had little uninterrupted time or compute. I wanted to leave work in a folder.
A local model could process it when the machine was available. I could return
to ordinary files later. Existing local tools depended on a live chat or
request. nrvna makes the work durable instead.

Local compute is finite and intermittent. Work should not disappear because
the caller, model process, or terminal is gone.

Local model servers are useful for chat and interactive completion. nrvna is
for work that should not depend on a live request or on the process waiting
for it:

- Queue a batch before you load a model.
- Let specialized models take turns on constrained hardware.
- Preserve work across terminal, caller, and daemon restarts.
- Inspect each input, result, and failure.
- Collect work from another script or agent session.

The model process is temporary. The workspace is the durable record.

## Files are the state

```text
workspace/
├── input/ready/   queued
├── processing/    claimed
├── output/        successful jobs and artifacts
└── failed/        terminal failures and error.txt
```

Atomic filesystem renames publish, claim, and complete jobs. After a crash,
the next daemon recovers abandoned jobs from `processing/`.

Execution is at least once. nrvna preserves terminal failures. The caller owns
the retry policy.

## Fresh context

Every `wrk` submission receives a new model context. Context does not carry
between jobs.

`--parent` records lineage only. It does not copy context, wait for another
job, or impose execution order. Put prior evidence needed by a new job into
its prompt explicitly.

Job isolation makes recovery possible. A new daemon can reconstruct each job
from files without hidden session state.

## Work it can run

| Work | Submit with | Primary artifact |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Embedding | `--embed` | `embedding.json` |
| Vision | `--image` | `result.txt` |
| Speech to text | `--audio ... --stt` | `transcript.txt` |
| Text to speech | `--tts` | `audio.wav` |

Vision and speech models can require an `mmproj`. TTS can require a vocoder.
Place matching files beside the model. `nrvnad` detects them automatically.

## Structured output

Constrain one text or vision job with JSON Schema:

```json
{"type":"object","properties":{"answer":{"type":"string"}},"required":["answer"]}
```

Save the schema as `answer.schema.json`. Then submit and drain the job:

```bash
JOB=$(./build/wrk "$WS" "Return the answer as JSON" \
  --json-schema answer.schema.json)
./build/nrvnad "$MODEL" "$WS" --drain
./build/flw "$WS" "$JOB" --json
```

GBNF is llama.cpp's grammar format. `wrk` converts the schema to GBNF before
it publishes the job. The job keeps both `schema.json` and the effective
`grammar.gbnf`. Use `--grammar <file>` when you already have GBNF. The two
options are mutually exclusive.

`result.txt` contains the generated JSON. `flw --json` returns it in the
`result` string and reports `output_format`.

## Give it to an agent

Paste this into Codex CLI, Claude Code, OpenCode, Pi, Hermes, OpenClaw, or
another agent with shell access:

```text
Read https://raw.githubusercontent.com/sanmathigb/nrvna/main/AGENTS.md.
Explain nrvna's job, context, drain, and failure contracts before using it.
Use an isolated workspace and an existing local model. Do not download models
or modify existing workspaces without asking me first.
```

`AGENTS.md` is the operational contract: stdout, JSON, exit codes, tags,
lineage, daemon lifecycle, artifacts, and recovery.

## Applications built with nrvna

- [imgsrch](apps/imgsrch/README.md) searches local screenshots by visible
  words and meaning. It coordinates caption, OCR, and embedding models through
  three durable workspaces.
- [bckbrnr](apps/bckbrnr/README.md) runs local prompt work from the macOS menu
  bar and writes answers back as files.

The applications call `nrvnad`, `wrk`, and `flw` directly. They add product
behavior. They do not add another inference path.

imgsrch is the first proof. Its current 3.4 GB model set has run end to end on
the 2017 Intel MacBook used to build nrvna. It can resume unfinished indexing
from persisted jobs. This is a compatibility result, not a performance
benchmark.

## Boundaries

nrvna is not a chat API, agent framework, model router, semantic index, or
distributed queue.

It does not assemble parent context, execute DAGs, choose models, parse
documents, validate model output, or retry failures automatically.

llama.cpp owns model inference. The calling application owns orchestration and
product behavior. nrvna keeps the local work durable between them.

## Documentation

- [Documentation map](DOCUMENTATION.md): the shortest path to the right depth
- [Quickstart](QUICKSTART.md): guided use of the primitives
- [Agent guide](AGENTS.md): machine and agent contract
- [Advanced patterns](ADVANCED.md): batches, chaining, and multiple models
- [Configuration](CONFIGURATION.md): runtime and application settings
- [Architecture](ARCHITECTURE.md): ownership and state transitions

## Build support

nrvna currently builds on macOS and Linux with CMake 3.16+ and a C++17
compiler. CPU inference is the default. Supported llama.cpp GPU backends can
be enabled with `NRVNA_GPU_LAYERS`.

MIT licensed. Model licenses remain model-specific.
