# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/llama.cpp-00fa7cb-orange.svg)](https://github.com/ggml-org/llama.cpp/commit/00fa7cb284cbf133fc426733bd64238a3588a33e)

Unix-like primitives for durable local inference. No always-on server.

Submit work before any model process runs. Run the model when compute is
available. Read ordinary files later.

Give `nrvnad` a GGUF model and a directory. That directory becomes the
workspace. Each job is a folder inside the workspace. Moving that folder
changes the job state. Results remain in the workspace as files.

![A terminal demo that submits a job while no daemon is running and shows one
queued job](assets/submit-without-daemon.gif)

Install the prebuilt binaries:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

The installer does not download a model. See [INSTALL.md](INSTALL.md) for the
manual archive path. See [QUICKSTART.md](QUICKSTART.md) to build from source.

State is location:

```text
input/writing/ -> input/ready/ -> processing/ -> output/
                                         \-> failed/
```

The workspace remembers. The model does not.

Each job uses a fresh model context. The workspace stores jobs and results.

[llama.cpp](https://github.com/ggml-org/llama.cpp) loads and runs the GGUF
models. nrvna adds durable jobs, workspaces, process lifecycle, and file-based
composition.

## Run one job

Use the example model from [QUICKSTART.md](QUICKSTART.md), or another compatible
instruction-tuned GGUF for this exact-output check:

```bash
job=$(wrk ./workspace "Reply with exactly: first")
nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain
flw ./workspace "$job"
```

```text
first
```

`wrk` creates the workspace and stores the job. `--drain` loads the model,
processes the job, and exits. The result remains under
`./workspace/output/$job/`.

**Experimental developer preview.** Tests cover the filesystem and lifecycle
contracts. nrvna does not claim production readiness.

<details>
<summary>Need an example model?</summary>

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

## Queue work before the model starts

```bash
first=$(wrk ./batch "Write one sentence about durable files")
second=$(wrk ./batch "Write one sentence about local models")

nrvnad ./models/smollm2-1.7b.gguf ./batch --drain

flw ./batch "$first"
flw ./batch "$second"
```

`wrk` stores both jobs before a model process exists. `--drain` loads the
model, processes the queued jobs, and exits.

Use separate workspaces for different model roles. Drain them in sequence when
their models cannot share memory.

For repeated low-latency work, run the same command without `--drain` in a
separate terminal. Stop that daemon with `nrvnad stop ./batch`.

Shell applications can source [`scripts/nrvna-lib.sh`](scripts/nrvna-lib.sh).
`nrvna_start` starts a daemon or uses one that is already starting. It waits
for readiness and reports startup failures from the daemon log.

## The work outlives the process

![A terminal demo that kills nrvnad, shows the claimed job on disk, and
recovers that job](assets/crash-recovery.gif)

I ran the `v0.1.1` release test on a 2017 Intel MacBook Pro. The test used the
SmolLM2 1.7B Q4_K_M model from the quick start. I sent `SIGKILL` while the job
was in `processing/`. The next daemon recovered that job.

```text
before SIGKILL  {"queued":0,"running":1,"done":0,"failed":0}
after restart   {"queued":0,"running":0,"done":1,"failed":0}
result          hello
recovery_attempts 1
```

The job completed. This is a lifecycle check, not a performance benchmark.

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
├── input/writing/  staging
├── input/ready/   queued
├── processing/    claimed
├── output/        successful jobs and artifacts
└── failed/        terminal failures, error.txt, and response.txt
```

Atomic filesystem renames publish, claim, and complete jobs. After a crash,
the next daemon recovers abandoned jobs from `processing/`. Each orphaned job
tracks `recovery_attempts` in `meta.json`; if it keeps crashing, the next
daemon moves it to `failed/` instead of looping forever.

The state directories and artifact names are compatibility commitments. Change
them only with a documented migration.

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
JOB=$(wrk "$WS" "Return the answer as JSON" \
  --json-schema answer.schema.json)
nrvnad "$MODEL" "$WS" --drain
flw "$WS" "$JOB" --json
```

The job keeps `schema.json` and its effective `grammar.gbnf`. Invalid JSON
fails before publication. The failed job preserves `error.txt` and its partial
`response.txt`. See the [agent machine contract](AGENTS.md#machine-contract)
and `wrk --help` for GBNF and retrieval details.

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

Non-goals:

- not a chat interface
- not an agent framework
- not an orchestrator
- not a model router
- not a semantic index
- not a distributed queue

It does not assemble parent context, execute DAGs, choose models, parse
documents, or retry failures automatically. Structured JSON jobs are
validated before publication, but general model output is still caller-owned.
Failed structured jobs preserve their partial `response.txt` for inspection.

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
