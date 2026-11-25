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

**Experimental developer preview.** nrvna runs on macOS and Linux, and it is not
production-ready. The primitives are small and the design is settled, but it is
lightly exercised. It needs more users, more machines, more models, more use
cases, and far more jobs actually run. Try it, report what breaks, and check its
output before you rely on it.

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

```sh
wrk <workspace> "<prompt>"      # save a job, print its ID (no model needed)
nrvnad <workspace> <model>      # process the workspace (--drain exits when idle)
flw <workspace> <job>           # read status and result
```

That is the whole interface: submit, process, read. A job is a file in the
workspace, so it waits there until a model runs it.

A runnable example - read the text out of a screenshot:

```sh
job=$(wrk ./ws "Transcribe the text." --image screenshot.png)
nrvnad ./ws vision-model.gguf --mmproj mmproj.gguf --drain
flw ./ws "$job"
```

The work type is a flag on `wrk`:

| Flag | Work | Result |
| --- | --- | --- |
| *(prompt only)* | text | `result.txt` |
| `--image <file>` | vision | `result.txt` |
| `--audio <file>` | transcription | `transcript.txt` |
| `--tts` | speech | `audio.wav` |
| `--embed` | embedding, for search | `embedding.json` |

Submit one job or a thousand. Each result is a durable file. Stop if submission
or inference fails, and check model output against its source before you use it.

## How it's used

Pick a mode for the work:

- **Drain** - run a batch and exit. Best for bounded jobs, swapping models, and
  tight hardware. `nrvnad ./ws model.gguf --drain`
- **Service** - keep a model loaded for repeated, low-latency work.
  `nrvnad ./ws model.gguf`. Submit from any terminal, a script, or your phone
  over Tailscale. nrvna exposes no port, so the only remote path is your own SSH.

Because it is CLI commands over a file contract, anything that runs a command or
reads a file can build on it, in any language, with no SDK or API:

- **Apps** - wrap the commands in a product. The app owns the experience; nrvna
  owns the durable work.
- **Workflows and recipes** - a script, a cron job, or a Makefile that composes
  `wrk`, `nrvnad`, and `flw` into a repeatable pipeline.
- **Agents** - hand an agent [AGENTS.md](AGENTS.md); it uses the commands directly.

This release ships the primitives. Apps built on nrvna ship and are announced
separately; anyone can build one on the same three commands.

See [composition examples](ADVANCED.md), [work types](docs/how-it-works.md#work-types),
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
