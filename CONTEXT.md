# nrvna Domain Language

Use these terms in code, documentation, and reviews. A disagreement between
the code and this file is a defect.

## Core Terms

| Term | Meaning |
| --- | --- |
| **Primitive** | One nrvna command: `wrk`, `nrvnad`, or `flw`. |
| **Workspace** | One directory that contains jobs and daemon state. One daemon can own it at a time. |
| **Job** | One directory that contains an independent inference task. |
| **State** | The job's current state directory. |
| **Claim** | An atomic rename from `input/ready/<id>` to `processing/<id>`. |
| **Artifact** | The primary output file from a successful job. |
| **Job contract** | The public rules for job states, IDs, types, and artifacts. |
| **Lifecycle contract** | The private rules for daemon status and control files. |

## Job States

| State | Directory | Meaning |
| --- | --- | --- |
| **Staging** | `input/writing/` | `wrk` is writing the job. Other processes cannot use it. |
| **Queued** | `input/ready/` | The job is complete and waits for a worker. |
| **Running** | `processing/` | A worker claimed the job. |
| **Done** | `output/` | The job completed and contains an artifact. |
| **Failed** | `failed/` | The job ended with `error.txt`. |
| **Missing** | none | No state directory contains the job ID. |

`wrk` publishes a staged job with one atomic rename. A worker claims a queued
job with another atomic rename. The job's directory is its state.

## Artifacts

A successful job has one primary artifact. `flw` resolves it in this order:

```text
result.txt -> transcript.txt -> audio.wav -> embedding.json
```

`include/nrvna/contract.hpp` defines the job contract. Applications use `wrk`
and `flw` instead of reading workspace directories. imgsrch still reads some
engine artifacts inside its private collection path.

## Language Boundary

The engine uses C++17. Applications use their native language:

- Go for portable command-line applications.
- Swift for macOS applications.
- Bash for small composition scripts.

Applications cross the nrvna boundary through the three commands. Do not add
an FFI, language binding, or second engine language. Tracked programs do not
use Python as a runtime dependency.

## Daemon Lifecycle

`include/nrvna/lifecycle.hpp` defines the lifecycle contract. It covers
`.nrvnad.lock`, `.nrvnad.pid`, `.nrvnad.ready`, and `.nrvnad.info`.

Use `nrvnad status` and `nrvnad stop`. Do not read lifecycle files to determine
daemon state. Use `--drain` when the daemon must process queued work and exit.
