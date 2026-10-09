# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/llama.cpp-v0.4.1-orange.svg)](https://github.com/ggml-org/llama.cpp/commit/b29c606e28a01b1bc8c1351026a0fa6e616bf6c4)

Unix-like primitives for durable local inference. No always-on server.

Submit work to a directory. Bring in a model when you want. Read the saved
results later. No model needs to be running when you submit.

That directory is the workspace. Each job is a folder inside it. nrvna moves
the folder as the job changes state. Inputs and results stay there as files.

![A terminal demo that submits a job while no daemon is running and shows one
queued job](assets/submit-without-daemon.gif)

## Three primitives

| Command | Contract |
| --- | --- |
| `wrk` | Save one independent job and print its ID |
| `nrvnad` | Load one model and process one workspace |
| `flw` | Inspect status or read results |

[llama.cpp](https://github.com/ggml-org/llama.cpp) loads and runs GGUF models.
nrvna adds durable jobs, workspaces, and process lifecycle.

## Start

Install the prebuilt binaries:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Use a compatible instruction-tuned GGUF. [INSTALL.md](INSTALL.md) includes a
verified example model, manual archive steps, and source build steps.

`wrk` creates the workspace when needed. Submit two smoke-test jobs without
waiting for either answer:

```bash
first=$(wrk ./workspace "Reply with exactly: first")
second=$(wrk ./workspace "Reply with exactly: second")
nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain
flw ./workspace "$first"
flw ./workspace "$second"
```

`--drain` processes queued work and exits when idle. The results remain readable
after the model exits. Submission does not wait for inference.

For useful work, save instructions and source notes in `meeting.md`:

```markdown
Extract the owners and the publication blocker in three short bullets.

Maya reviews the draft. Ben checks the screenshots.
Do not publish until both reviews are complete.
```

For repeated work, keep the model available. Check status first; start it only
when no daemon owns this workspace:

```bash
nrvnad status ./workspace
nrvnad ./models/smollm2-1.7b.gguf ./workspace &
job=$(wrk ./workspace - < meeting.md)
flw ./workspace -w "$job"
nrvnad stop ./workspace
```

The shell sends the file's text to `wrk`. `flw -w` waits for this job's result.
Omit the wait and retrieve it later if you prefer. Each submission has fresh
context; include all required evidence in its input.

**Experimental developer preview.** Tests cover the filesystem and lifecycle
contracts. nrvna does not claim production readiness.

## State is location

```text
input/writing/ -> input/ready/ -> processing/ -> output/
                                         \-> failed/
```

The workspace remembers. The model does not.

Atomic renames publish, claim, and complete jobs. The next daemon recovers
jobs left in `processing/`. Repeated recovery stops at a fixed ceiling and
moves the job to `failed/`.

An interrupted job starts again with fresh context. Token generation does not
resume. Execution is at least once: a job may run more than once after a crash.
The caller decides whether to retry jobs that end in `failed/`.

## The work outlives the process

![A terminal demo that kills nrvnad, shows the claimed job on disk, and
recovers that job](assets/crash-recovery.gif)

I ran this check with the `v0.1.1` release on a 2017 Intel MacBook Pro. I sent
`SIGKILL` while one job was in `processing/`. The next daemon recovered it.

```text
before SIGKILL  {"queued":0,"running":1,"done":0,"failed":0}
after restart   {"queued":0,"running":0,"done":1,"failed":0}
result          hello
recovery_attempts 1
```

This is a process-crash check, not a speed benchmark or a power-loss guarantee.

## Why

I built nrvna on a 2017 Intel MacBook while caring for two young children. My
time and compute were both interrupted. I wanted to submit work, leave, and
read the results later.

Once the work is at the center, the intelligence doesn't have to be.
A caller, terminal, or model process can stop. The work should remain.

## Work types

| Work | Submit with | Result |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Embedding | `--embed` | `embedding.json` |
| Vision | `--image` | `result.txt` |
| Speech to text | `--audio` (or `-a`) | `transcript.txt` |
| Text to speech | `--tts` | `audio.wav` |

Vision and speech transcription need compatible models and projector files.
Text to speech needs a compatible model and vocoder. Use a separate workspace
for each model role. One model does not necessarily support every work type.

Use `wrk --json-schema <file>` for schema-constrained text or vision output.
The job preserves the schema and effective grammar. Invalid JSON fails before
publication and keeps the partial response for inspection.

## Give it to an agent

Paste this into an agent with shell access:

```text
Read https://raw.githubusercontent.com/sanmathigb/nrvna/main/AGENTS.md.
Explain nrvna's job, context, drain, and failure contracts before using it.
Use an isolated workspace and an existing local model. Do not download models
or modify existing workspaces without asking me first.
```

`AGENTS.md` defines stdout, JSON, exit codes, tags, lineage, daemon lifecycle,
artifacts, and recovery.

## Applications

- [Verified file organizer example](examples/file-organizer/README.md) asks a
  model for a move plan, then validates the plan before files move.
- [imgsrch](apps/imgsrch/README.md) searches local screenshots by visible
  words and meaning. It uses caption, OCR, and embedding workspaces.
- [bckbrnr](apps/bckbrnr/README.md) runs local prompt work from the macOS menu
  bar and writes answers as files.

These applications add product behavior. They use the same three primitives.

## Boundaries

nrvna is not a chat interface, agent framework, orchestrator, model router,
semantic index, or distributed queue.

`--parent` records lineage only. It does not copy context, wait for another
job, or set execution order.

The caller owns dependencies, model selection, document parsing, search, and
retries of failed jobs. nrvna saves and processes the work; applications decide
what work to submit and what to do with the results.

## Reference

- [Install](INSTALL.md): binaries, example model, and source build
- [Agent guide](AGENTS.md): operational and machine contract
- [Domain language](CONTEXT.md): canonical terms
- [Advanced patterns](ADVANCED.md): composition examples
- [Configuration](CONFIGURATION.md): runtime settings
- [Architecture](ARCHITECTURE.md): ownership and state transitions

nrvna builds on macOS and Linux with CMake 3.16+ and C++17. CPU inference is
the default. Supported llama.cpp GPU backends use `NRVNA_GPU_LAYERS`.

MIT licensed. Model licenses remain model-specific.
