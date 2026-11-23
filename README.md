# nrvna

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Built on llama.cpp](https://img.shields.io/badge/built%20on-llama.cpp-orange.svg)](https://github.com/ggml-org/llama.cpp/tree/b1b770d90)

Local inference through three independent commands.

nrvna separates prompt submission from inference. `wrk` saves work without
waiting for a model. `nrvnad` processes that work. `flw` reads status and results.
This separation makes the work asynchronous.

| Command | Purpose |
| --- | --- |
| `wrk` | Save a job in a workspace and return its ID. |
| `nrvnad` | Load a local model and process jobs from that workspace. |
| `flw` | Read job status and saved results. |

Run each command on its own, like a Unix utility. They share files in a
workspace directory. Jobs and results remain after the commands exit.
Each job starts with fresh context.

Keep a daemon running to process incoming jobs, or use `--drain` to process
the queue and exit. Submission works in either mode, or without a daemon.

**Experimental developer preview.** nrvna runs on macOS and Linux. It does
not claim production readiness.

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

The installer does not download a model. Follow [Install](INSTALL.md) to choose
a model, get an optional checksummed text example, or build from source.

The `PATH` change applies to this terminal. Add it to your shell startup file
for future terminals.

## Use the three commands

Use a new workspace. Submit two independent jobs:

```sh
wrk ./workspace "Summarize: Sales rose 8%. Costs rose 3%." --tag first-run
wrk ./workspace "Extract the deadline: Send the draft by Friday." --tag first-run
flw ./workspace --json
```

Each submission prints a job ID. Both jobs are queued. No model is loaded.
The prompts include the source text, so no input file is needed.

Check daemon status before starting inference:

```sh
nrvnad status ./workspace --json
```

For a new workspace, status reports not running and exits with code `1`.
Replace `/path/to/model.gguf` below with a compatible local text model.
Process the queue and exit:

```sh
nrvnad ./workspace /path/to/model.gguf --drain
```

Read both results as one JSON object per line:

```sh
flw ./workspace --tag first-run --json
```

To save one result as text, replace `JOB_ID` with its submission ID:

```sh
flw ./workspace JOB_ID > result.txt
```

For ongoing work, omit `--drain`. Keep that daemon running in one terminal.
Use `wrk` and `flw` from another terminal. Submission still returns without
waiting for inference. See [the persistent-daemon example](ADVANCED.md#submit-to-a-running-daemon-and-collect-later).

Stop if submission or inference fails. Inspect the workspace with `flw`.
Check model output against its source before using it.

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

The workspace holds job state and results. Text, embeddings, transcripts, and
generated audio remain files under `output/<job-id>/`. After a process crash, a
new daemon can recover abandoned work. A recovered job starts again; token
generation does not resume. Execution is at least once, with a recovery ceiling.

Use a separate workspace for each model role. Put prior results in the next
prompt when needed. `--parent` records lineage; it does not pass context or
control execution order.

The caller owns model selection, document extraction, workflow order, retries,
and result validation. nrvna provides independent durable inference jobs.

## Why

Prompt submission should not depend on inference. Work should remain available
when the caller exits or the model stops. Files provide that shared record.

- [Asynchronous. Inference. Primitives.](https://sanmathigb.com/asynchronous-inference-primitives/)
- [GAI before AGI?](https://sanmathigb.com/generallyavailableintelligence/)

## Reference

[Documentation](docs/README.md) · [Agent guide](AGENTS.md) ·
[Configuration](CONFIGURATION.md) · [Architecture](ARCHITECTURE.md) ·
[Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md) ·
[Security](SECURITY.md)

Built with C++17 and llama.cpp. MIT licensed. Model licenses remain
model-specific.
