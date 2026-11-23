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

The installer does not download a model. Follow [Install](INSTALL.md) to choose
a model, get an optional checksummed text example, or build from source.

The `PATH` change applies to this terminal. Add it to your shell startup file
for future terminals.

## Submit now, run later

Put source text in `report.txt`. Use a new workspace for this example. Submit
the file contents as one job:

```sh
JOB=$(
  {
    printf '%s\n' 'Summarize this report in one sentence:'
    cat report.txt
  } | wrk ./workspace -
)
```

`wrk` prints a job ID. No model needs to run during submission. The job stays
in the workspace until a daemon processes it.

Set `MODEL` to a local GGUF that supports your task. Then process the job:

```sh
MODEL=/path/to/model.gguf
nrvnad ./workspace "$MODEL" --drain
```

`--drain` waits until the queue is idle, then exits. Save the result as text:

```sh
flw ./workspace "$JOB" > summary.txt
```

`summary.txt` is the model output. The workspace keeps the job and its result.
To inspect job status and metadata, use:

```sh
flw ./workspace "$JOB" --json
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
