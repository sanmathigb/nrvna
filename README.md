# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggml-org/llama.cpp/tree/8642541b255bfb5dc39cd351a178be09013a55d6)

Durable local AI work, built from three commands.

nrvna lets you submit work without waiting for inference. Keep submitting as
new work arrives. Run a persistent daemon or drain a batch. Read results when
ready. Jobs and results stay as files in a workspace. Agents can use the
commands directly or follow the [nrvna skill](skills/nrvna/SKILL.md).

[First run](#start-with-the-commands) · [How it works](docs/how-it-works.md) · [Pro use](ADVANCED.md) · [Agents](AGENTS.md)

`wrk` creates jobs. `nrvnad` runs them with a local model. `flw` reads their
status and results. Each job has fresh context. The caller chooses what to
include.

Models get `wrk`. You get `flw`.

![A terminal demo that submits a job while no daemon is running and shows one
queued job](assets/submit-without-daemon.gif)

## Start with the commands

Install the prebuilt commands:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

This changes `PATH` only in this terminal. Run it in each new terminal, or add
it to your shell startup file.

Use an existing compatible GGUF model, or follow [INSTALL.md](INSTALL.md) to
download and verify the example model. The commands below use
`./models/smollm2-1.7b.gguf`.

Run one job:

```bash
wrk ./workspace "Summarize this report in five bullets: Sales rose 8%, costs rose 3%. Name one risk."
nrvnad ./workspace ./models/smollm2-1.7b.gguf --drain
flw ./workspace JOB_ID
```

Replace `JOB_ID` with the ID printed by `wrk`. `--drain` loads the model,
processes queued work, then exits. Results remain in the workspace.

## Give this to your agent

Paste this into an agent with shell access. Replace the model and workspace
paths, then describe the work.

```text
Read nrvna's agent guide:
https://raw.githubusercontent.com/sanmathigb/nrvna/main/AGENTS.md

Use my local GGUF model: /path/to/model.gguf
Use a new workspace: /path/to/workspace

Run the task as one independent job. Include all required instructions and
evidence. Do not download a model or reuse an existing workspace. Use the
documented commands. Report the job ID, result, and any error.

Task: [describe the work]
```

## Choose how the model runs

Use `--drain` for bounded batches, first runs, or model swaps. It loads the
model, processes queued work, and exits when the workspace is idle.

For repeated work, keep one daemon running for the workspace. Check its status:

```bash
nrvnad status ./workspace
```

Status exits `0` when ready, `2` while starting, and `1` when stopped. Start a
daemon only when status exits `1`:

```bash
nrvnad ./workspace ./models/smollm2-1.7b.gguf
```

The daemon stays in the foreground. In another terminal, submit work and copy
the job ID printed by `wrk`:

```bash
wrk ./workspace "Summarize this report"
flw ./workspace JOB_ID -w
```

Replace `JOB_ID` with the ID from `wrk`. Stop the daemon when you finish:

```bash
nrvnad stop ./workspace
```

Both modes use the same workspace and durable job files. See
[INSTALL.md](INSTALL.md) for setup.

## Work in. Results out.

```text
input/ready/ → processing/ → output/
                         └→ failed/
```

The workspace stores each job and its result as files.

Atomic renames publish and claim jobs. After a crash, the next daemon can
recover an abandoned job from its saved input. It starts again with fresh
context. Token generation does not resume. Execution is at least once, so a
job may run more than once after a crash. Recovery stops at a fixed limit.

![A terminal demo that kills nrvnad, shows the claimed job on disk, and
recovers that job](assets/crash-recovery.gif)

The process-crash check used the `v0.1.1` release on a 2017 Intel MacBook Pro.
It does not prove recovery from power loss or hardware failure.

## Work types

| Work | Submit with | Result |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Embedding | `--embed` | `embedding.json` |
| Vision | `--image` | `result.txt` |
| Speech to text | `--audio` (or `-a`) | `transcript.txt` |
| Text to speech | `--tts` | `audio.wav` |

`wrk --image` attaches image files to a job. Repeat it to attach more images.
nrvna does not import or index folders. Submit each image as a separate job
when you need a separate result for each image.

Some work types need matching projector or vocoder files. Use a separate
workspace for each model role. See [CONFIGURATION.md](CONFIGURATION.md).

LFM3 text, vision, and audio models use llama.cpp support added upstream in
October 2026. Vision and audio jobs need the matching `--mmproj` file. LFM3
audio models use that projector for speech output. Older TTS models use a
vocoder. nrvna does not yet accept video or mixed image-and-audio jobs.
Liquid d1 decision models are also outside the current text-generation job
interface.

Each job gets fresh context. Put required instructions and evidence in its
input. `--parent` records lineage only. It does not pass context or set order.

## Why

I wanted to submit work, leave, and read the results later. The lifetime of
the work should not be tied to the runtime of a model. More on the thinking:
[Asynchronous. Inference. Primitives.](https://sanmathigb.com/asynchronous-inference-primitives/)
and [GAI before AGI?](https://sanmathigb.com/generallyavailableintelligence/)

## Compose with the shell

Combine `wrk`, `nrvnad`, and `flw` with files, processes, and other Unix tools.
Use the shell to choose the model, pass results between jobs, and handle errors.

## What nrvna does not do

nrvna is not a chat interface, agent framework, model router, search index, or
distributed queue. It does not parse documents or retry failed jobs.

The caller chooses models, selects context, composes jobs, and handles failed
work. nrvna saves and runs independent inference jobs.

## Status

**Experimental developer preview.** Tests cover the filesystem and lifecycle
contracts. nrvna does not claim production readiness.

## Reference

- [Install](INSTALL.md): binaries, example model, and run modes
- [Changelog](CHANGELOG.md): user-visible changes and breaking CLI updates
- [Documentation](docs/README.md): guides and reference pages
- [Why nrvna](docs/why.md): the problem shape behind independent durable jobs
- [Contributing](CONTRIBUTING.md): build and review expectations
- [Security](SECURITY.md): private vulnerability reports
- [Agent guide](AGENTS.md): operational and machine contract
- [Domain language](CONTEXT.md): canonical terms
- [Advanced patterns](ADVANCED.md): composition examples
- [Configuration](CONFIGURATION.md): runtime settings
- [Architecture](ARCHITECTURE.md): ownership and state transitions

nrvna builds on macOS and Linux with CMake 3.16+ and C++17. CPU inference is
the default. Model licenses remain model-specific.

MIT licensed.
