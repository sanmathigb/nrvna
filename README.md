# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/llama.cpp-00fa7cb-orange.svg)](https://github.com/ggml-org/llama.cpp/commit/00fa7cb284cbf133fc426733bd64238a3588a33e)

Unix-like primitives for durable local inference. No always-on server.

Give `nrvnad` a GGUF model and a directory. That directory becomes the
workspace. Each job is a folder inside it. Moving that folder changes the job
state. Results stay there as files.

![A terminal demo that submits a job while no daemon is running and shows one
queued job](assets/submit-without-daemon.gif)

## Start

Install the prebuilt binaries:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Use a compatible instruction-tuned GGUF. [INSTALL.md](INSTALL.md) includes a
verified example model, manual archive steps, and source build steps.

Load the model only when work is ready:

```bash
job=$(wrk ./workspace "Reply with exactly: first")
nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain
flw ./workspace "$job"
```

The result is:

```text
first
```

Keep the model ready when low latency matters:

```bash
nrvnad ./models/smollm2-1.7b.gguf ./workspace &
job=$(wrk ./workspace "Reply with exactly: first")
flw ./workspace -w "$job"
nrvnad stop ./workspace
```

`wrk` and `flw` work the same way in both modes.

**Experimental developer preview.** Tests cover the filesystem and lifecycle
contracts. nrvna does not claim production readiness.

## Three primitives

| Command | Contract |
| --- | --- |
| `wrk` | Publish one independent job and print its ID |
| `nrvnad` | Load one model and process one workspace |
| `flw` | Inspect status or read results |

[llama.cpp](https://github.com/ggml-org/llama.cpp) loads and runs GGUF models.
nrvna adds durable jobs, workspaces, process lifecycle, and file composition.

## State is location

```text
input/writing/ -> input/ready/ -> processing/ -> output/
                                         \-> failed/
```

The workspace remembers. The model does not.

Each job uses a fresh model context. `--parent` records lineage only. It does
not copy context, wait for another job, or set execution order.

Atomic renames publish, claim, and complete jobs. The next daemon recovers
jobs left in `processing/`. Repeated recovery stops at a fixed ceiling and
moves the job to `failed/`.

Execution is at least once. Write jobs whose repetition is safe. The caller
owns retry policy.

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

This is a lifecycle check. It is not a performance benchmark.

## Why

I built nrvna on a 2017 Intel MacBook while caring for two young children. My
time and compute were both interrupted. I wanted to submit work, leave, and
read the results later.

Local compute is finite. A caller, terminal, or model process can stop. The
work should remain.

## Work types

| Work | Submit with | Result |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Embedding | `--embed` | `embedding.json` |
| Vision | `--image` | `result.txt` |
| Speech to text | `--audio ... --stt` | `transcript.txt` |
| Text to speech | `--tts` | `audio.wav` |

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

- [imgsrch](apps/imgsrch/README.md) searches local screenshots by visible
  words and meaning. It uses caption, OCR, and embedding workspaces.
- [bckbrnr](apps/bckbrnr/README.md) runs local prompt work from the macOS menu
  bar and writes answers as files.

These applications add product behavior. They use the same three primitives.

## Boundaries

nrvna is not a chat interface, agent framework, orchestrator, model router,
semantic index, or distributed queue.

It does not assemble parent context, execute dependency graphs, choose models,
parse documents, search artifacts, or retry failures. llama.cpp owns model
inference. The calling application owns orchestration and product behavior.

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
