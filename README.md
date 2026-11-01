# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggml-org/llama.cpp/tree/8642541b255bfb5dc39cd351a178be09013a55d6)

Durable local AI work, built from three commands.

nrvna gives agents and apps a durable way to run local inference. Jobs,
inputs, results, and failures live as files in a workspace. Run a bounded batch
with `--drain`, or keep a daemon ready for repeated work. Agents can use the
commands directly or follow the [nrvna skill](skills/nrvna/SKILL.md). Apps can
compose the same primitives into complete workflows.

[Try nrvna](#start-with-the-commands) · [How it works](docs/how-it-works.md) · [Build an app](#build-on-it)

`wrk` creates jobs. `nrvnad` runs them with a local model. `flw` reads their
status and results. Each job has fresh context; the caller chooses what to
include.

![A terminal demo that submits a job while no daemon is running and shows one
queued job](assets/submit-without-daemon.gif)

## Start with the commands

Install the prebuilt commands:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Use a compatible instruction-tuned GGUF model. [INSTALL.md](INSTALL.md) has a
verified example, manual archive steps, and source build instructions.

Run two independent jobs in one bounded batch:

```bash
wrk ./workspace "Summarize this report in five bullets: Sales rose 8%, costs rose 3%. Name one risk." --tag report
wrk ./workspace "List two decisions from this note: Ship on Friday. Mira owns the final test." --tag report
nrvnad ./workspace ./models/smollm2-1.7b.gguf --drain
flw ./workspace --tag report --json
```

`wrk` creates the workspace when needed. `--drain` loads the model, processes
the queued jobs, then exits. Results remain in the workspace.

## Choose how the model runs

Use `--drain` for bounded batches, first runs, or model swaps. It loads the
model, processes queued work, and exits when the workspace is idle.

For repeated work, keep one daemon running for the workspace. Check its status
first. Use `nrvnad stop <workspace>` when you finish. Both modes use the same
workspace and durable job files. See [INSTALL.md](INSTALL.md) for setup.

## Work stays on disk

```text
input/ready/ → processing/ → output/
                         └→ failed/
```

The workspace remembers. The model does not.

Atomic renames publish and claim jobs. After a crash, the next daemon can
recover an abandoned job from its saved input. It starts again with fresh
context; token generation does not resume. Execution is at least once, so a
job may run more than once after a crash. Recovery stops at a fixed ceiling.

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

Some work types need matching projector or vocoder files. Use a separate
workspace for each model role. See [CONFIGURATION.md](CONFIGURATION.md).

LFM3 text, vision, and audio models use llama.cpp support added upstream in
October 2026. Vision and audio jobs need the matching `--mmproj` file. LFM3
audio models use that projector for speech output; older TTS models use a
vocoder. nrvna does not yet accept video or mixed image-and-audio jobs.
Liquid d1 decision models are also outside the current text-generation job
interface.

Each job gets fresh context. Put required instructions and evidence in its
input. `--parent` records lineage only; it does not pass context or set order.

## Why

I wanted to submit work, leave, and read the results later. The lifetime of
the work should not be tied to the runtime of a model. More on the thinking:
[Asynchronous. Inference. Primitives.](https://sanmathigb.com/asynchronous-inference-primitives/)
and [GAI before AGI?](https://sanmathigb.com/generallyavailableintelligence/)

## Build on it

Use `wrk`, `nrvnad`, and `flw` from a shell, or install the skill so a supported
coding agent can call them from a conversation. Build a focused app when a
workflow needs more than the primitives.

- [File organizer](examples/file-organizer/README.md) validates a model's move
  plan before it moves files.
- [imgsrch](apps/imgsrch/README.md) searches images by visible words and
  meaning. It uses caption, OCR, and embedding jobs.
- [bckbrnr](apps/bckbrnr/README.md) submits local work from the macOS menu bar
  and saves answers as files.

These applications add product behavior. They use the same three commands.

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
