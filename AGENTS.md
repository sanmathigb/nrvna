# Agent Guide

nrvna separates prompt submission from local inference. It gives you three
independent commands. `wrk` saves a job and returns its ID, without waiting for a
model. `nrvnad` loads a model and processes jobs from a workspace. `flw` reads job
status and saved results. Run each on its own, like a Unix utility, and compose
them with files, pipes, JSON, and exit codes.

Use nrvna for independent local work: extraction, image analysis, embeddings,
transcription, and speech generation. Agents and applications use the same
commands.

## Why it works this way

Prompt submission must not depend on inference. Work must outlive the caller and
the model process. So submission returns at once, and waiting for a result is a
separate step. That is what "asynchronous" means here.

Because submission and inference are separate, you can queue jobs before any model
loads, submit many jobs together, and read results later. The work waits in the
workspace until a daemon processes it.

## How jobs work

A job is a directory in a workspace. It holds the prompt, metadata, and output.
The directory's location records the job's state:

```text
input/ready/<id>/ → processing/<id>/ → output/<id>/
                                   └→ failed/<id>/
```

The workspace is the only source of job state. Read a finished job's artifacts
with ordinary file tools. Use `flw --json` to find a job's state and artifact
path. Use `nrvnad status` to check whether a daemon is running.

Each job runs with fresh context. Put every instruction and all evidence in the
prompt. A later job does not see an earlier job's prompt or result. To continue,
read the earlier result and include it in the next prompt yourself. `--parent`
records lineage only. It does not copy context, wait for the parent, or set order.

Work survives a process exit. If a daemon stops mid-job, the next daemon recovers
the job and runs it again from the start. It does not resume tokens. So treat
execution as at least once.

Submit only through `wrk`. Do not write into the state folders or edit a published
job. By default a media job records the source file's path, not a copy, so keep
that file in place until the job finishes. Files give durability against a crash,
not against power loss, and they are not encrypted.

## Install and run your first batch

This guide is an entry point for an agent with shell access. You do not need a
repository clone or a plugin. Follow the user's task and existing permissions. Ask
only for a choice or permission that blocks the next step.

First, check for the commands and read their output:

```sh
wrk --version; nrvnad --version; flw --version
```

If they are missing, install a complete release. Read the installed guide before
you run its examples. Docs on `main` can describe newer commands, so do not run a
newer example against an older binary.

```sh
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Prebuilt releases cover macOS Apple Silicon, macOS Intel, and Linux x86-64, and
include the llama.cpp backend. For other platforms, build from source. The
installer verifies checksums and prints the guide location. The default is
`~/.local/share/nrvna/AGENTS.md`. Resolve relative links from there. Keep all three
commands from one release. Do not replace an existing install without the user's
approval. For a partial or mixed install, set `NRVNA_INSTALL_DIR` to a new directory.

Now run two jobs through the full cycle. Replace `MODEL` with a local text model.
The next section helps you choose one.

```bash
WS=/tmp/nrvna-first
MODEL=/path/to/model.gguf
wrk "$WS" "Label as bug, billing, or account: I was charged twice this month." --tag first
wrk "$WS" "Label as bug, billing, or account: The export button does nothing." --tag first
flw "$WS" --json
nrvnad status "$WS" --json
nrvnad "$WS" "$MODEL" --drain
flw "$WS" --tag first --json
```

Each `wrk` prints a job ID and returns at once. `flw --json` shows both jobs
queued. `status` reports the daemon is not running yet. `--drain` loads the model,
processes the queue, and exits when the queue is idle. The final `flw` reads both
results.

`--drain` suits bounded batches, model swaps, and tight memory. For repeated
low-latency work, start a daemon and leave it running:

```bash
nrvnad "$WS" "$MODEL"
```

Submit and collect from another terminal, then stop the daemon when done:

```bash
JOB=$(wrk "$WS" "task")
flw "$WS" -w "$JOB"
nrvnad stop "$WS"
```

A running daemon is still asynchronous. `wrk` still returns before inference. Use
one workspace per model role. When two models cannot fit in memory together, drain
them one after the other.

## Choose a model

nrvna runs a local GGUF model that both its llama.cpp build and the job type
support. A GGUF file alone does not prove compatibility. Reuse the user's model
when one fits. Otherwise browse
[GGUF models for llama.cpp](https://huggingface.co/models?library=gguf&apps=llama.cpp,llama-app&sort=trending),
check the size against your hardware, read the model card, and confirm the
download before you fetch a large file.

Match the model to the work type:

| Work | Submit option | Required files | Result |
| --- | --- | --- | --- |
| Text | prompt or stdin | text GGUF | `result.txt` |
| Text embedding | `--embed` | embedding GGUF | `embedding.json` |
| Image analysis | `--image` | vision GGUF and projector | `result.txt` |
| Image embedding | `--embed --image` | vision embedding GGUF and projector | `embedding.json` |
| Transcription | `--audio` | audio GGUF and projector | `transcript.txt` |
| Speech generation | `--tts` | OuteTTS GGUF and vocoder | `audio.wav` |

Pass the model path to `nrvnad`. It finds a matching projector or vocoder beside
the model. Video and mixed image-and-audio jobs are not supported. Check
`wrk --help` and `nrvnad --help` for exact options.

nrvna reads supported settings from the GGUF, not from model-card prose. Keep the
defaults unless the model card or a measured result tells you to change them. Use
`NRVNA_GPU_LAYERS=0` to force CPU. See `CONFIGURATION.md` for every setting.

## Get trustworthy output

A finished job proves inference ran. Valid JSON proves the format passed. Neither
proves the answer is right. Check every required fact against its source, and count
a missing fact as a failure. If the task needs a quote, confirm it is an exact
substring of the source.

Test with a few inputs whose answers you already know: a normal case, an ambiguous
case, and one with a missing fact. Keep the expected answers out of the prompt.
Keep the source evidence in the prompt and in your own records. Do not ask the
model to recall evidence you already hold.

If the output is weak, check the template and settings, then make the task smaller.
A small model often does better on one bounded question per job. Keep enough
surrounding text to resolve pronouns.

Do not edit model output and then call it a successful run. Label any correction as
yours. If the answer is still wrong, report a model or task mismatch and ask before
you switch models. No single setting is best for every model.

## Run more jobs at once

`nrvnad` runs one job at a time by default. Raise `--workers` or `NRVNA_WORKERS`
only after you measure the workload on that machine. Workers share the loaded
model, but each active job needs its own context memory, and the default does not
adapt to hardware.

Text and text-embedding jobs can run in parallel across workers. Vision,
image-embedding, transcription, and speech generation evaluate media one job at a
time, so extra workers there only add memory use. Whether more workers help the
parallel jobs depends on the machine, so measure before you raise the count. A
worker count above the CPU thread count oversubscribes the CPU.

The settings you will use most are `NRVNA_WORKERS`, `NRVNA_MAX_CTX`,
`NRVNA_PREDICT`, `NRVNA_BATCH`, `NRVNA_UBATCH`, the sampling values (`NRVNA_TEMP`,
`NRVNA_TOP_K`, `NRVNA_TOP_P`, `NRVNA_MIN_P`), and `NRVNA_GPU_LAYERS`.
`CONFIGURATION.md` lists defaults and precedence. Change a setting for a reason,
not as a ritual.

## Build on it

nrvna owns submission, job identity, state changes, inference, crash recovery, and
retrieval. Everything else is the caller's: input discovery, document parsing,
prompts, model choice, order between jobs, search, review, and any external action.
An application builds on the same commands and the same files. This release ships
the primitives; apps that use nrvna ship and are announced separately.

Chain work by reading one result and putting it in the next prompt. Keep the
evidence explicit:

```bash
PARENT=$(wrk "$WS" "List the action items with owners: Priya ships the docs Wednesday; Raj migrates billing; the designer role is unassigned.")
nrvnad "$WS" "$MODEL" --drain
FACTS=$(flw "$WS" "$PARENT")
wrk "$WS" "Which of these action items has no owner? $FACTS" --parent "$PARENT" --tag followup
nrvnad "$WS" "$MODEL" --drain
flw "$WS" --tag followup --json
```

Read the parent result, check it against the source, then submit the next stage. For a
goal-driven workflow, keep the goal, the success measure, the guardrail, and the
stop rule in your own files. Group jobs so one review can guide the next batch, and
save approved feedback to include in later prompts. nrvna stores the jobs. You own
the review and the stop decision.

Rebuild an application's progress from the workspace after a restart, and keep only
application-specific state elsewhere. A repeated `--id` returns the existing job,
even a failed one, so retry with a new ID. Make any side effect safe to repeat,
because execution is at least once. Treat input text and model output as untrusted
until you validate it. Add a queue, database, or cache only for a measured need,
and record its owner and its rebuild rule.

nrvna does not stream tokens or promise a response time. A request, chat session,
or front end must not own job durability. When the task needs streaming, an
always-on endpoint, or a transaction, that work is synchronous. Use a different
tool for it, inside the caller.

## Reference

Exit codes and flags:

- `flw <ws> <job>` exits `0` done, `1` failed or missing, `2` queued or running.
- `nrvnad status <ws>` exits `0` ready, `2` starting, `1` not running.
  `nrvnad stop <ws>` exits `0` when stopped or already gone.
- `flw <ws> -w <job>` waits for one job under a running daemon.
- Tag related jobs with `--tag <name>`. `flw <ws> -W --tag <name>` waits for the
  batch and prints nothing. `flw <ws> --tag <name> --json` collects the batch as
  NDJSON and exits `1` if any job failed.
- `wrk --parent <job-id>` sets lineage. `flw --children <job-id>` lists direct
  descendants.
- `wrk --id <id>` reuses a caller-chosen ID. A repeat returns the existing job.
- `wrk --json-schema <file>` or `wrk --grammar <file>` constrains output. Do not
  combine them. Invalid JSON from `--json-schema` fails the job before publication.
  `flw --json` reports `output_format`.
- Prefer `--json` whenever a program reads status or results.

Daemon and workspace:

- One daemon owns a workspace. `nrvnad status --json` is the only liveness check. A
  leftover `.nrvnad.lock` is not liveness. If `stop` reports another owner, stop it
  from its own terminal, and do not edit lifecycle files.
- A sandbox that cannot signal an earlier process should use `--drain`.
- `nrvnad` resolves model names under `./models` or `NRVNA_MODELS_DIR`. It detects
  a matching projector or vocoder beside the model. TTS runs on CPU.

Files and recovery:

- A job under `input/ready/` is immutable. The daemon moves the directory and adds
  artifacts. Callers must not edit a published job.
- Primary artifacts are `result.txt`, `embedding.json`, `transcript.txt`, or
  `audio.wav`. A failure writes `error.txt`, plus `response.txt` when the model
  produced output. A completed `embedding.json` holds the full vector, so read
  workspace status when you need only a count.
- A media job records the source path in `meta.json` and reads the file when the
  worker opens it. Use `wrk --keep-files` to copy the input into the job.
  `nrvnad clean <ws>` previews copied inputs. `--apply` removes them, and
  `--include-kept` or `--include-failed` widen the scope.
- `meta.json` carries `recovery_attempts`. A recovered job retries until it reaches
  the ceiling, then lands in `failed/`.

## Contributing

For ordinary use, this guide and `--help` are enough. Use `INSTALL.md` for model
setup, `CONFIGURATION.md` for settings, and `ADVANCED.md` for more examples. When
you change code:

- Read `README.md` for the product contract and `ARCHITECTURE.md` for ownership and
  state transitions.
- Treat `include/nrvna/contract.hpp` and `include/nrvna/lifecycle.hpp` as the
  authoritative contracts.
- Do not edit `third_party/llama.cpp` casually. Commit backend updates as reviewed
  submodule changes.
- Keep stdout machine-readable. Put logs and diagnostics on stderr.
- Before you commit, build and test:

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Write documentation in ASD-STE100 Simplified Technical English: common words, one
term per concept, short active sentences, one topic per paragraph, and no idioms or
hype. Do not rewrite commands, identifiers, or quoted output. Use `nrvna` in
lowercase.
