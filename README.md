# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggml-org/llama.cpp/tree/b1b770d90)

Durable local AI work, built from three commands.

| Command | Purpose |
| --- | --- |
| `wrk` | Submit a job and return its ID without waiting for inference. |
| `nrvnad` | Run queued jobs with a local model. |
| `flw` | Read job status and results. |

Submit work before loading a model, or while a daemon runs. Jobs and results
stay in a workspace directory after the commands exit. Each job starts with
fresh context.

**Experimental developer preview.** nrvna runs on macOS and Linux. It does
not claim production readiness.

## Install

Install the prebuilt commands for macOS Apple Silicon, macOS Intel, or Linux
x86-64:

```sh
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

The installer downloads the latest published release. Use the guides installed
with those commands. Docs on `main` can describe newer commands. Check `--help`
before using a command.

The installer does not download a model. Follow [Install](INSTALL.md) for a
verified example-model download, archive checksums, and source builds.
That guide creates `./models/smollm2-1.7b.gguf`, used below.

The `PATH` change applies to this terminal. Add it to your shell startup file
for future terminals.

## Submit now, run later

Use a new workspace for this example. Submit two independent jobs:

```sh
wrk ./workspace "Summarize in one sentence: Sales rose 8%. Costs rose 3%." --tag first-run
wrk ./workspace "Name one risk: Our only supplier has delayed two deliveries." --tag first-run
flw ./workspace --json
```

Both jobs are queued. No model needed to run during submission. Each `wrk`
command prints a job ID.

Process the queue with your downloaded model:

```sh
nrvnad ./workspace ./models/smollm2-1.7b.gguf --workers 1 --drain
```

`--drain` waits until the queue is idle, then exits. Read the batch results:

```sh
flw ./workspace --tag first-run --json
```

The output contains one JSON object per line. To read one result as text,
replace `JOB_ID` with an ID from submission:

```sh
flw ./workspace JOB_ID
```

A nonzero drain exit means the drain failed. Inspect the workspace with `flw`
before continuing. Model output needs review before you rely on it.

## Give it to your agent

Give your Codex, Claude, or other shell-capable agent [AGENTS.md](AGENTS.md)
and this task. The guide covers installation, model setup, first jobs, and
application design.

```text
Read the attached nrvna AGENTS.md. Install nrvna if needed.
Continue with the guide shipped with the installed release.
Use my local model: [path, or help me choose a compatible model].
Task: [describe the work or application].

Use independent durable jobs. Submit independent work before waiting.
Validate results and report job IDs, artifact paths, and failures.
```

You can attach the file, paste its contents, or share its link. Shell access
and normal download permissions are needed. The guide does not grant extra permissions.
After installation, the default guide is `~/.local/share/nrvna/AGENTS.md`.
The installer prints its location for custom installations.

Agents can also use the optional [nrvna skill](skills/nrvna/SKILL.md).
Applications use the same commands and durable file contract.

## Examples

[Three examples](ADVANCED.md) show how to:

- Submit a batch before loading the model.
- Submit to a running daemon and collect results later.
- Pass a completed result into a new job.

Text, images, embeddings, transcription, and speech generation use the same
job lifecycle. Each needs a compatible model. See [work types](docs/how-it-works.md#work-types)
and [model settings](INSTALL.md#use-model-specific-settings).

## The contract

```text
input/ready/ → processing/ → output/
                         └→ failed/
```

The workspace holds job state and results. After a process crash, a new daemon
can recover abandoned work. A recovered job starts again; token generation
does not resume. Execution is at least once, with a recovery ceiling.

Use a separate workspace for each model role. Put prior results in the next
prompt when needed. `--parent` records lineage; it does not pass context or
control execution order.

The caller owns model selection, document extraction, workflow order, retries,
and result validation. nrvna provides independent durable inference jobs.

## Why

I wanted to submit work, leave, and read the results later. The lifetime of
the work should not be tied to the runtime of a model.

- [Asynchronous. Inference. Primitives.](https://sanmathigb.com/asynchronous-inference-primitives/)
- [GAI before AGI?](https://sanmathigb.com/generallyavailableintelligence/)

## Reference

[Documentation](docs/README.md) · [Agent guide](AGENTS.md) ·
[Configuration](CONFIGURATION.md) · [Architecture](ARCHITECTURE.md) ·
[Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md) ·
[Security](SECURITY.md)

Built with C++17 and llama.cpp. MIT licensed. Model licenses remain
model-specific.
