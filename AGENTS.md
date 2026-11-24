# Agent Guide

nrvna separates prompt submission from local inference. It has three independent
commands. `wrk` saves a job and returns its ID. It does not wait for inference.
`nrvnad` processes jobs from a workspace. `flw` reads job status and saved
results. Run each command on its own, like a Unix utility. Compose them with
files, pipes, JSON, and exit codes.

Keep a daemon running for incoming jobs, or use `--drain` to process the queue
and exit. Neither mode changes how `wrk` submits work. The workspace holds jobs
and results after the commands exit.

Every `wrk` submission creates an independent durable job. A repeated `--id`
returns the existing job. Context does not continue between jobs. `--parent`
records lineage only. It does not copy context, wait for the parent, or control
order.

## Why nrvna Exists

Prompt submission must not depend on inference. Work must outlive the caller and
the model process. Asynchronous means submission does not wait for inference.
Waiting for results is a separate step.

Use nrvna for independent local work: extraction, image analysis, embeddings,
transcription, and speech generation. Agents and applications use the same
commands. Each job needs a compatible model and complete input evidence.

## The Design

- **Files hold work.** A job directory holds its prompt, metadata, and artifacts.
  Its location records its state. The workspace is the source of job state.
- **Submission and execution are separate.** Queue jobs before you load a model.
  Submit independent jobs together. Wait only when you need their results.
- **Work survives process exits.** The next daemon recovers interrupted jobs.
  Recovery restarts inference. It does not resume tokens. Execution is at least once.
- **Each job has fresh context.** Put instructions and evidence in every prompt.
  Pass selected results into later jobs. Parents record lineage only.
- **Models have separate roles.** Use one workspace per role. Drain roles in
  sequence when their models do not fit in memory together.
- **Callers compose the work.** Use files, pipes, JSON, and exit codes. Keep
  product policy outside the primitives.

A persistent daemon reduces repeated model loads. It still accepts independent
asynchronous jobs. A user interface can show progress without holding submission
open. Do not make a request, chat session, or frontend process own job durability.

## Use the Files

```text
input/ready/<id>/ → processing/<id>/ → output/<id>/
                                   └→ failed/<id>/
```

Read completed artifacts with ordinary file tools. An application can index
embeddings, show saved image descriptions, or play generated audio. Use
`flw --json` to find state and artifact paths. Use `nrvnad status` for liveness.

Keep prompts and results available for inspection and later stages. Rebuild
application progress from job records after a restart. Read selected artifacts as
needed. Do not load a whole collection into memory.

Submit through `wrk`. Do not write into state folders or edit published jobs.
Reference inputs stay external unless you copy them with `--keep-files`. A
workspace backup must account for those sources. Files do not imply a power-loss
guarantee, encryption, or safe deletion of referenced inputs.

## Keep the Boundary

The primitives own submission, job identity, state transitions, inference,
process-crash recovery, and result retrieval.

Callers own input discovery, document parsing, prompts, model choice, dependency
order, search, review, and external actions. Terminal-failure retries are caller
policy. Interrupted-job recovery is a primitive mechanism.

Before you add a queue, database, cache, or router, check the existing file
contract. Add an index only for a measured need. State its source and its rebuild
rule. Keep application behavior outside the core.

Build applications around independent jobs and saved results. A persistent daemon
is still asynchronous. Submission does not wait for inference. nrvna does not
stream generated tokens or guarantee response times. Use another interface when
the task needs those features. Keep application transactions and external actions
in the caller.

## Install From This Guide

This file is an entry point for an agent with shell access. A repository clone and
an agent plugin are not required. Follow the user's task and existing permissions.
Ask only for a missing choice or a permission that blocks the next step.

1. Check whether `wrk`, `nrvnad`, and `flw` are installed.
2. Read each installed command's `--version` and `--help` output.
3. If commands are missing, install a complete release with the installer below.
4. Read the installed guide before you run its examples.

```sh
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Prebuilt releases support macOS Apple Silicon, macOS Intel, and Linux x86-64. They
include the llama.cpp backend. No separate llama.cpp installation is needed. For
other platforms, use the release's source-build instructions.

The installer verifies archive checksums and prints the guide location. The
default is `~/.local/share/nrvna/AGENTS.md`. Resolve relative links from that
directory, not from the caller's project. Keep all three commands from one
release. Do not replace an existing installation without user authorization. For a
partial or mixed installation, set `NRVNA_INSTALL_DIR` to a new directory and use
all three binaries from it.

If the installed release lacks this guide, use its tagged repository documentation
and installed help. Do not apply newer examples to older binaries. The
[release page](https://github.com/sanmathigb/nrvna/releases) provides archives and
versioned source. Models are separate downloads. Reuse a compatible local model.

## Rules for You

- Use lowercase `nrvna`.
- Do not invent flags or commands.
- Treat `--parent` as lineage only.
- Check `nrvnad status` before you tell anyone to start a daemon.
- Use `--drain` when the sandbox cannot signal a live daemon.
- Keep stdout machine-readable. Put diagnostics on stderr.
- Do not tell anyone to poll background work.
- Use one workspace per model role.
- Put prior evidence in the next prompt when a job needs it.
- Jobs are directories. Models are GGUF files. Do not confuse them.
- Do not describe nrvna as a server, orchestrator, or agent framework.

## Start Here

1. Read each command's `--version` and `--help` before you use the examples.
2. Use the guide shipped with those binaries. Do not mix binaries and
   instructions from different revisions.
3. Confirm the user's model and workspace. Do not download or replace a model
   without authorization.
4. Check `nrvnad status <workspace> --json` before you start a daemon.
5. Put each task's instructions and evidence in its prompt. A file path alone
   does not supply its contents.
6. Submit independent jobs before you wait. Save their IDs. Use a tag to collect
   the batch.
7. Run a drain, or use a running daemon and a `flw` wait. Collect on completion,
   not by repeated status checks.
8. Validate results against the source. Report failed jobs, missing facts, and
   unsupported claims.

## Choose a Model

nrvna uses local GGUF models that its bundled llama.cpp revision and job type
support. A GGUF file alone does not establish compatibility.

1. Reuse the user's compatible model when one is available.
2. Otherwise, inspect local hardware and the requested work type.
3. Browse [GGUF models for llama.cpp](https://huggingface.co/models?library=gguf&apps=llama.cpp,llama-app&sort=trending).
4. Add hardware in [Hugging Face settings](https://huggingface.co/settings/hardware) to see memory estimates.
5. Read the model card, license, required files, and recommended settings.
6. Confirm the download size and model choice when you are not already authorized.

Hardware estimates do not guarantee speed or compatibility. Use the installed
`CONFIGURATION.md` for supported settings. Begin with the model author's
recommendations. Validate changes on representative inputs.

| Work | Submit option | Required model files | Primary result |
| --- | --- | --- | --- |
| Text | prompt or stdin | Compatible text GGUF | `result.txt` |
| Text embedding | `--embed` | Compatible embedding GGUF | `embedding.json` |
| Image analysis | `--image` | Vision GGUF and matching projector | `result.txt` |
| Image embedding | `--embed --image` | Vision embedding model and projector | `embedding.json` |
| Transcription | `--audio` | Audio model and projector | `transcript.txt` |
| Speech generation | `--tts` | OuteTTS model and vocoder | `audio.wav` |

These are separate work types, not capabilities of every model. Video and mixed
image-and-audio jobs are not supported. Check `wrk --help` for submission syntax.
Check `nrvnad --help` for projector and vocoder options.

Pass local model paths to `nrvnad`. It detects a matching projector and vocoder
beside the model. nrvna reads supported GGUF settings and templates, not
model-card prose. Keep the runtime defaults unless model-specific guidance or a
measured result supports an override. Use `NRVNA_GPU_LAYERS=0` to force CPU
inference.

## Set Runtime Concurrency

`nrvnad` defaults to one concurrent job. Set `--workers` or `NRVNA_WORKERS` to
change the worker-pool size. Workers share the loaded model. Each active job needs
its own context and compute memory. The default does not adapt to hardware.

Raise the count only after you measure the workload on that machine. The pool
accepts every job type, but not every type runs inference in parallel. Vision,
image-embedding, and STT media evaluation is serialized. LFM3 TTS generation is
serialized. More workers can add memory use without improving those stages. A
count above the CPU thread count can still oversubscribe the CPU, because each
worker gets at least one thread.

The main controls are concurrency (`NRVNA_WORKERS`), context and output limits
(`NRVNA_MAX_CTX`, `NRVNA_PREDICT`), prompt processing and memory (`NRVNA_BATCH`,
`NRVNA_UBATCH`), sampling (`NRVNA_TEMP`, `NRVNA_TOP_K`, `NRVNA_TOP_P`,
`NRVNA_MIN_P`), and device placement (`NRVNA_GPU_LAYERS`). Use `CONFIGURATION.md`
for exact defaults and precedence. Do not change every setting as a tuning ritual.

## Build Goal Workflows

The useful unit is a complete piece of work that finishes without the caller open.
A job needs a running daemon to make progress. Durable does not mean always
running.

Keep goal rules in caller-owned files. Record the outcome, the success measure,
the guardrail measure, the review batch, the checkpoint, and the stop or
escalation rule. Group jobs so one review lesson can guide later jobs. Save
approved feedback in a file. Include it in later prompts. The caller owns review,
metrics, schedules, and stop decisions. nrvna stores jobs and preserves results.

A later stage reads selected results and submits a new complete prompt. Choose
that evidence yourself. Do not assume conversation history, dependency order, or
parent context.

## Get Useful Results

Start with a few representative inputs whose answers you can check. Include missing
facts, ambiguous inputs, and a normal case. Keep expected answers out of the
prompt. Use fresh inputs to check a change to your prompt or settings.

A successful job means inference completed. Valid JSON means the format passed.
Neither proves the answer is correct or complete. Check every required fact and
its source. Count omissions as failures. Keep original evidence in the prompt and
in caller records. Do not ask the model to rebuild evidence you already have. If
the task needs quotes, verify them as exact source substrings.

If quality is poor, check the template and effective settings. Then simplify the
task. A small model may work better on one bounded question per job. Keep enough
surrounding evidence to resolve pronouns and shared context.

Submit independent jobs together. Do not turn the batch into a serial conversation.
Retry with a new job ID and keep the earlier evidence. Do not silently correct
model output and call it a successful run. Label caller corrections separately.
Report a model or task mismatch when validation still fails. Ask before you switch
models. Never claim one parameter set is best for all models.

## Build Applications From the Same Contract

- Submit through `wrk`. Keep each returned ID before you show work as accepted.
- Rebuild progress from job state and results after a restart. Keep only
  application state elsewhere.
- Use tags for selection and stable IDs for repeat submission. Put input and
  operation versions in the ID recipe.
- A repeated ID returns the existing job, including a failure. Retry on purpose
  with a new ID.
- Treat execution as at least once. Make application side effects safe to repeat.
- Reference inputs must stay available and unchanged. Copy them with `--keep-files`
  when you need a snapshot.
- Validate results before file moves, edits, or other actions. Treat input text
  and generated text as untrusted data.
- Use `flw` for job state and `nrvnad status` for daemon state. Never use a
  leftover lock file as a liveness check.
- Add queues, indexes, and caches only for a measured need. State their owner and
  recovery rule.

## Cold Run

Use a text model for this first check. Replace `MODEL` with its local path. The
default installer uses `~/.local/bin`. Source builds use `./build`. Extracted
archives use `./bin`. Set `BIN` to your binary directory.

```bash
BIN="$HOME/.local/bin"
WS=/tmp/nrvna-agent
MODEL=./models/model.gguf
JOB1=$("$BIN/wrk" "$WS" "Reply with exactly: first" --tag quickstart)
JOB2=$("$BIN/wrk" "$WS" "Reply with exactly: second" --tag quickstart)
"$BIN/flw" "$WS" --json
"$BIN/nrvnad" status "$WS" --json
"$BIN/nrvnad" "$WS" "$MODEL" --workers 2 --drain
"$BIN/flw" "$WS" "$JOB1" --json
"$BIN/flw" "$WS" "$JOB2" --json
```

`wrk` creates the workspace when needed. It prints only the job ID on stdout.
`wrk --id <id>` reuses a caller-chosen safe ID. A repeat returns the existing job,
including a failure. Use a new ID for a retry. Before the daemon starts, workspace
status shows two queued jobs. Daemon status reports not running for a new
workspace. `--drain` loads the model and starts the configured workers. It exits
when the queue is idle. A nonzero exit can mean two things. The drain operation
failed, or a job ended in terminal failure. Check job status with `flw` before you
retry. Do not resubmit the batch for a single terminal failure. No process must
stay open.

nrvna runs in two modes. Drain mode processes the queued jobs and then exits. Use
it for bounded batches, model swapping, and constrained hardware. Service mode
keeps a daemon running for repeated low-latency work. Both modes accept
independent asynchronous jobs. Start a service daemon in one terminal:

```bash
"$BIN/nrvnad" "$WS" "$MODEL"
```

In another terminal, set the same `BIN` and `WS`, then submit and collect:

```bash
JOB=$("$BIN/wrk" "$WS" "task")
"$BIN/flw" "$WS" -w "$JOB"
"$BIN/nrvnad" stop "$WS"
```

Use a separate workspace for each model role. Drain them in sequence when their
models do not fit in memory together.

## Machine Contract

- Prefer `--json` when you consume status or results programmatically.
- `flw <ws> <job>` exits `0` when done, `1` when failed or missing, and `2` when
  queued or running.
- `nrvnad status <ws>` exits `0` when ready, `2` while starting, and `1` when not
  running. `nrvnad stop <ws>` exits `0` when stopped or already absent.
- Use `flw <ws> -w <job>` to wait for one job handled by a persistent daemon.
- Add the same `--tag <name>` to each related `wrk` command.
- Use `flw <ws> -W --tag <name>` as the batch barrier. It prints no results.
- Collect the batch with `flw <ws> --tag <name> --json`. It emits NDJSON. It exits
  `1` when a selected job failed.
- Use `wrk --parent <job-id>` for lineage and `flw --children <job-id>` to read
  direct descendants.
- One daemon owns a workspace at a time. `nrvnad status <ws> --json` is the
  liveness check. `nrvnad stop <ws>` is the graceful stop.
- Some agent sandboxes cannot signal a process that an earlier tool call started.
  Use `--drain` in these sandboxes.
- If `stop` reports an owner, stop the daemon from its terminal. Do not edit
  lifecycle files.
- A `.nrvnad.lock` file may remain after exit. Its existence is not liveness. Use
  `nrvnad status`, not lifecycle files.
- Jobs become immutable under `input/ready/`. The daemon moves the job directory
  and adds artifacts. Callers must not edit published jobs.
- Image and audio jobs record absolute source paths in `meta.json` by default.
  Keep those files available until the job finishes, including for recovery. Use
  `wrk --keep-files` to copy and retain inputs inside the job.
- Reference jobs read the file contents present when the worker opens the path.
  nrvna does not detect source changes or preserve the original contents.
- Successful jobs keep prompts, metadata, and results. Failed reference jobs keep
  the source paths. Copied inputs stay with the job. `nrvnad clean` removes copied
  attachments only.
- Failures are terminal and preserved under `failed/`, with `error.txt` and a
  `response.txt` when the model produced output, even if that file is empty. Retry
  policy belongs to the caller, not the primitive.
- Primary artifacts are `result.txt`, `embedding.json`, `transcript.txt`, or
  `audio.wav`. Failures use `error.txt`. Completed embedding JSON includes the full
  vector, so use workspace status rather than job retrieval when you need only
  counts.
- `nrvnad clean <ws>` previews copied attachments in terminal jobs. Add `--apply`
  to remove eligible files. `--include-kept` and `--include-failed` widen scope.
- Use `wrk --json-schema <file>` for schema-constrained text or vision output. Use
  `wrk --grammar <file>` for existing GBNF. Do not combine these options.
- Structured jobs preserve `schema.json` when provided and always preserve the
  effective `grammar.gbnf`. Invalid JSON from a `--json-schema` job fails the job
  before publication. `flw --json` reports `output_format`.

Put required prior evidence in the next prompt. Keep lineage separate.

```bash
PARENT=$("$BIN/wrk" "$WS" "Extract the facts: Sales rose 8%. Costs rose 3%.")
"$BIN/nrvnad" "$WS" "$MODEL" --drain
"$BIN/flw" "$WS" "$PARENT" > facts.txt
```

Continue only if submission, execution, and retrieval succeeded. Check `facts.txt`
against the source before you submit the next stage.

```bash
{ echo "Name one unanswered question about these facts:"; cat facts.txt; } \
  | "$BIN/wrk" "$WS" - --parent "$PARENT" --tag followup
"$BIN/nrvnad" "$WS" "$MODEL" --drain
"$BIN/flw" "$WS" --tag followup --json
```

`wrk` stages each submission and publishes it with an atomic rename. Workers also
claim jobs with an atomic rename. The daemon publishes terminal output under
`output/` or `failed/`. After a crash, the next daemon recovers abandoned
`processing/` jobs. The job's `meta.json` carries `recovery_attempts`. A recovered
job can run again until it hits the recovery ceiling. Then it lands in `failed/`.
Only one terminal job directory becomes visible. Treat execution as at least once.

Model names resolve under `./models` or `NRVNA_MODELS_DIR`. `nrvnad` detects
matching mmproj and vocoder files beside the model. llama.cpp estimates GPU
placement when the build supports it. Set `NRVNA_GPU_LAYERS=0` to force CPU. TTS
models and vocoders use CPU placement. Use `nrvnad --help` for job types and model
requirements.

nrvna does not assemble parent context or execute dependency graphs. It does not
route models, parse documents, retry failures, or search artifacts. Callers own
orchestration, context selection, validation, and search.

## Working On This Repository

For ordinary operation, start with this guide and the commands' `--help` output.
Use `INSTALL.md` for model setup, `CONFIGURATION.md` for settings, and
`ADVANCED.md` for composition examples.

When you change code, also follow these rules:

- Read `README.md` for the product contract.
- Read `ARCHITECTURE.md` for ownership and state transitions.
- Treat `include/nrvna/contract.hpp` and `include/nrvna/lifecycle.hpp` as the
  authoritative filesystem and daemon contracts.
- Do not edit `third_party/llama.cpp` casually. Backend updates are isolated,
  reviewed against the upstream examples, and committed as submodule changes.
- Keep stdout machine-readable. Diagnostics and logs belong on stderr.
- Before you commit core changes, run:

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## Writing

Use ASD-STE100 Simplified Technical English for technical documents and procedures.

- Use common, precise words. Use one term for one concept.
- Keep instructions at 20 words or fewer. Use active voice.
- Write short paragraphs. Put one topic in each paragraph.
- Avoid idioms, filler, hype, and jargon. Define a technical term before you use it.
- Do not rewrite commands, identifiers, file names, or quoted output.
- Use `nrvna` in lowercase unless an identifier requires another form.

Apply the same clarity to comments, interface text, commits, reports, and agent
replies. Do not call these surfaces formally ASD-STE100 compliant. Formal
compliance needs the official controlled dictionary and validated vocabulary and
grammar checks.
