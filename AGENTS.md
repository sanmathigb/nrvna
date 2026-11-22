# Agent Guide

nrvna provides durable local inference through three commands. `wrk` submits
jobs. `nrvnad` executes them. `flw` reads results. nrvna is not a chat API or
agent framework. Compose it with processes, exit codes, JSON, and files. At its
core, nrvna is a durable job queue built on atomic renames and files: no broker,
no database, no server.

Every new `wrk` submission creates an independent durable job. A repeated
`--id` returns the existing job. Context does not continue between jobs.
`--parent` records lineage only. It does not copy context, wait for the parent,
or control execution order.

## Why nrvna Exists

Submit work, leave, and read results later. Work must outlive the caller and
the model process. Asynchronous means submission returns before inference
finishes. Waiting is a separate operation.

Use nrvna for independent local work: extraction, image analysis, embeddings,
transcription, and speech generation. Agents and applications use the same
commands. Each job needs a compatible model and complete input evidence.

## The Design

- **Files hold work.** A job directory holds its prompt, metadata, and artifacts.
  Its location records its state. The workspace is the source of job state.
- **Submission and execution are separate.** Queue jobs before loading a model.
  Submit independent jobs together. Wait only when their results are needed.
- **Work survives process exits.** The next daemon recovers interrupted jobs.
  Recovery restarts inference; it does not resume tokens. Execution is at least once.
- **Each job has fresh context.** Put instructions and evidence in every prompt.
  Pass selected results explicitly into later jobs. Parents record lineage only.
- **Models have separate roles.** Use one workspace per role. Drain roles in
  sequence when their models cannot fit in memory together.
- **Callers compose the work.** Use files, pipes, JSON, and exit codes.
  Keep product policy outside the primitives.

A persistent daemon reduces repeated model loads. It still accepts independent
asynchronous jobs. A UI can show progress without holding submission open.
Do not make a request, chat session, or frontend process own job durability.

## Use the Files

```text
input/ready/<id>/ → processing/<id>/ → output/<id>/
                                   └→ failed/<id>/
```

Read completed artifacts with ordinary file tools. For example, an app can
index embeddings, display saved images' descriptions, or play generated audio.
Use `flw --json` to find state and artifact paths. Use `nrvnad status` for liveness.

Keep prompts and results available for inspection and later stages. Rebuild
app progress from job records after restart. Read selected artifacts as needed;
do not load an entire collection into application memory.

Submit through `wrk`. Do not write into state folders or edit published jobs.
Reference inputs remain external files unless copied with `--keep-files`.
A workspace backup must account for those sources. Files do not imply a
power-loss guarantee, encryption, or safe deletion of referenced inputs.

## Keep the Boundary

The primitives own submission, job identity, state transitions, inference,
process-crash recovery, and result retrieval.

Callers own input discovery, document parsing, prompts, model choice, dependency
order, search, review, and external actions. Terminal-failure retries are caller
policy. Interrupted-job recovery is a primitive mechanism.

Before adding a queue, database, cache, or router, check the existing file
contract. Add an index only for a measured need. Define its source and rebuild rule.
Keep application-specific behavior outside the core. Propose a shared mechanism
only when independent callers need it; document its contract and recovery behavior.

Build applications on the asynchronous layer. Use that work and results are
durable files that accumulate without a server. If a need is real-time,
streaming, always-on, or transactional, that need is synchronous. nrvna is not
that layer. Do not add a server, queue, or database to the primitives to reach
it. Tell the user plainly, and recommend a synchronous tool for that need.

## Install From This Guide

This file is an entry point for an agent with shell access. A repository clone
and an agent plugin are not required. Follow the user's task and existing permissions.
Ask only for missing choices or permissions that block the next step.

1. Check whether `wrk`, `nrvnad`, and `flw` are installed.
2. Read each installed command's `--version` and `--help` output.
3. If commands are missing, install a complete release with the installer below.
4. Read the installed guide before running its examples.

```sh
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Prebuilt releases support macOS Apple Silicon, macOS Intel, and Linux x86-64.
They include the llama.cpp backend. No separate llama.cpp installation is needed.
For other platforms, consult the release's source-build instructions before proceeding.

The installer verifies archive checksums and prints the guide location.
The default is `~/.local/share/nrvna/AGENTS.md`. Resolve relative document links
from that directory, not from the caller's project. Keep all three commands
from one release. Do not replace an existing installation without user authorization.
For partial or mixed installations, set `NRVNA_INSTALL_DIR` to a new directory
when running the installer. Use all three binaries from that directory.

If the installed release lacks this guide, use its tagged repository documentation
and installed help. Do not apply newer examples to older binaries.
The [release page](https://github.com/sanmathigb/nrvna/releases) provides archives
and versioned source. Models are separate downloads; reuse a compatible local model.

## Rules for You

- Use lowercase `nrvna`.
- Do not invent flags or commands.
- Treat `--parent` as lineage only.
- Check `nrvnad status` before telling anyone to start a daemon.
- Use `--drain` when the sandbox cannot signal a live daemon.
- Keep stdout machine-readable. Put diagnostics on stderr.
- Do not tell anyone to poll background work.
- Use one workspace per model role.
- Put prior evidence in the next prompt when a job needs it.
- Jobs are directories. Models are GGUF files. Do not conflate them.
- Do not describe nrvna as a server, orchestrator, or agent framework.

## Start Here

1. Read the installed commands' `--version` and `--help` output before using examples.
2. Use the guide shipped with those binaries. Do not combine release binaries with instructions from another revision.
3. Confirm the user's model and workspace. Do not download models or replace their chosen model without authorization.
4. Check `nrvnad status <workspace> --json` before starting a daemon.
5. Put each task's instructions and evidence in its prompt. A file path alone does not supply its contents.
6. Submit independent jobs before waiting. Save their IDs and use a tag to collect the batch.
7. Run a drain, or use the existing daemon and a `flw` wait. Collect on completion, not repeated status checks.
8. Validate the results against the source. Report failed jobs, missing facts, and unsupported claims explicitly.

The published `v0.1.1` uses `nrvnad <model> <workspace>`. This source revision
uses `nrvnad <workspace> <model>`. Installed help decides which syntax applies.
New options such as `--id` require binaries built from a revision that supports them.

## Choose a Model

nrvna uses local GGUF models supported by its bundled llama.cpp revision and
job type. A GGUF file alone does not establish compatibility.

1. Reuse the user's compatible model when available.
2. Otherwise, inspect local hardware and the requested work type.
3. Browse [GGUF models for llama.cpp](https://huggingface.co/models?library=gguf&apps=llama.cpp,llama-app&sort=trending).
4. Optionally add hardware in [Hugging Face settings](https://huggingface.co/settings/hardware) to see memory estimates.
5. Read the model card, license, required files, and recommended settings.
6. Confirm the download size and model choice when not already authorized.

Hardware estimates do not guarantee speed or compatibility with this release.
A model card describes the model's intended use, requirements, and limitations.
Use the installed `CONFIGURATION.md` for supported settings. Begin with the
model author's recommendations; validate changes on representative inputs.

| Work | Submit option | Required model files | Primary result |
| --- | --- | --- | --- |
| Text | prompt or stdin | Compatible text GGUF | `result.txt` |
| Text embedding | `--embed` | Compatible embedding GGUF | `embedding.json` |
| Image analysis | `--image` | Vision GGUF and matching projector | `result.txt` |
| Image embedding | `--embed --image` | Supported vision embedding model and projector | `embedding.json` |
| Transcription | `--audio` | Supported audio model and projector | `transcript.txt` |
| Speech generation | `--tts` | Supported OuteTTS model and vocoder | `audio.wav` |

These are separate work types, not capabilities of every model. Video and
mixed image-and-audio jobs are not supported. Check `wrk --help` for submission
syntax. Check `nrvnad --help` for projector and vocoder options.

Pass local model paths to `nrvnad`. It detects matching projector and vocoder
files beside the model. nrvna reads supported GGUF settings and templates,
not model-card prose. Set explicit overrides with environment variables:
`NRVNA_TEMP`, `NRVNA_TOP_P`, `NRVNA_TOP_K`, and `NRVNA_MIN_P`. Use a low
temperature such as `NRVNA_TEMP=0.3` for extraction and other exact tasks.
Use `NRVNA_GPU_LAYERS=0` for CPU inference. See the installed `CONFIGURATION.md`
for all settings.

## Think in Jobs

The useful unit is a complete piece of work that can finish without the caller
staying open. Submit independent pieces together. Waiting belongs at the point
where another task needs their results.

The workspace owns job state. The model process and the calling application
can stop without deleting that record. A job needs a running daemon to make
progress; durable does not mean always running.

A later stage reads selected results and submits a new complete prompt. Choose
that evidence explicitly. Do not assume conversation history, dependency order,
or parent context. Keep one workspace per model role.

## Get Useful Results

Read [model setup](INSTALL.md#use-model-specific-settings) before a new model's
first workload. Check its upstream model card, template, required files, and
recommended generation settings. nrvna reads supported GGUF hints, not model-card prose.

Start with a few representative inputs whose answers you can check. Include
missing facts, ambiguous inputs, and a normal case. Keep expected answers out
of the model prompt. Use fresh inputs to check changes to your prompt or settings.

A successful job means inference completed. Valid JSON means the format passed.
Neither proves that the answer is correct or complete. Check every required
fact and its source; count omissions as failures. Keep original evidence in
the prompt and caller records. Do not ask the model to reconstruct evidence
already available as input. If the task needs selected quotes, verify them as
exact source substrings before accepting them.

If quality is poor, check the template and effective settings, then simplify
the task. A small model may work better on one bounded question per job.
Keep enough surrounding evidence to resolve pronouns and shared context. Do
not split away information the job needs. Preserve source text when rewriting
it adds no value.

Submit those independent jobs together; do not turn the batch into a serial
conversation. Retry with new job IDs and retain the earlier evidence.

Do not silently correct model output and call it a successful model run. Label
caller corrections separately. Report a model/task mismatch if validation still
fails; ask before switching models. Never claim one parameter set is best for all models.

## Build Applications From the Same Contract

- Submit through `wrk`; retain each returned ID before showing work as accepted.
- Rebuild progress from job state and results after restart. Keep only application-specific state elsewhere.
- Use tags for selection and stable IDs for repeat submission when supported. Include input and operation versions in the ID recipe.
- Repeating an ID returns the existing job, including failure. Retry deliberately with a new ID.
- Treat execution as at least once. Make any application-side effects safe to repeat.
- Reference inputs must stay available and unchanged. Copy them with `--keep-files` when the application needs a snapshot.
- Validate results before file moves, edits, or other application actions. Treat input text and generated text as untrusted data.
- Use `flw` for job state and `nrvnad status` for daemon state. Never use a leftover lock file as a liveness check.
- Add queues, indexes, and caches only for a measured need. State their owner and recovery rule.

## Cold Run

Use a text model for this first check. Replace `MODEL` with its local path.
The default installer uses `~/.local/bin`; source builds use `./build`.
Extracted archives use `./bin`. Set `BIN` to your binary directory.

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
`wrk --id <id>` reuses a caller-chosen safe ID. A repeat returns the existing
job, including failures. Use a new ID for a retry.
Before the daemon starts, workspace status shows two queued jobs. Daemon status
should report not running for this new workspace. `--drain` loads the
model and starts the two configured workers. It exits when it observes an idle
queue. A nonzero exit can mean two things: the drain operation failed, or a job
ended in terminal failure. Check job status with `flw` before you retry. Do not
resubmit the batch for a single terminal job failure. No process must stay open.

nrvna runs in two modes. Drain mode processes the queued jobs and then exits;
use it for bounded batches, model swapping, and constrained hardware. Service
mode keeps a daemon running for repeated low-latency work. Both modes accept
independent asynchronous jobs. Start a service daemon in one terminal:

```bash
"$BIN/nrvnad" "$WS" "$MODEL"
```

In another terminal, set the same `BIN` and `WS` values, then submit and collect:

```bash
JOB=$("$BIN/wrk" "$WS" "task")
"$BIN/flw" "$WS" -w "$JOB"
"$BIN/nrvnad" stop "$WS"
```

Use separate workspaces for different model roles. Drain them in sequence when
their models cannot fit in memory together.

## Machine Contract

- Prefer `--json` when consuming status or results programmatically.
- `flw <ws> <job>` exits `0` when done, `1` when failed or missing, and `2`
  when queued or running.
- `nrvnad status <ws>` exits `0` when ready, `2` while starting, and `1` when
  not running. `nrvnad stop <ws>` exits `0` when stopped or already absent.
- Use `flw <ws> -w <job>` to wait for one job handled by a persistent daemon.
- Add the same `--tag <name>` to each related `wrk` command.
- Use `flw <ws> -W --tag <name>` as the batch barrier. It prints no results.
- Collect the batch with `flw <ws> --tag <name> --json`. It emits NDJSON. It
  exits `1` when a selected job failed.
- Use `wrk --parent <job-id>` for lineage and `flw --children <job-id>` to read
  direct descendants.
- One daemon owns a workspace at a time. `nrvnad status <ws> --json` is the
  liveness check; `nrvnad stop <ws>` is the graceful stop operation.
- Some agent sandboxes cannot signal a process that an earlier tool call
  started. Use `--drain` in these sandboxes.
- If `stop` reports an owner, stop the daemon from its terminal. Do not edit
  lifecycle files.
- A `.nrvnad.lock` file may remain after exit. Its existence is not liveness;
  do not inspect lifecycle files instead of using `nrvnad status`.
- Jobs become immutable under `input/ready/`. The daemon moves the job
  directory and adds artifacts. Callers must not edit published jobs.
- Image and audio jobs record absolute source paths in `meta.json` by default.
  Keep those files available until the job finishes, including for recovery.
  Use `wrk --keep-files` to copy and retain inputs inside the job.
- Reference jobs read the file contents present when the worker opens the path.
  nrvna does not detect source changes or preserve the original contents.
- Successful jobs keep prompts, metadata, and results. Failed reference jobs
  keep the source paths; copied inputs stay with the job. `nrvnad clean` removes
  copied attachments only.
- Failures are terminal and preserved under `failed/`, with `error.txt` and
  `response.txt` when the model produced output, even if that file is empty.
  Retry policy belongs to the calling app or agent, not the primitive.
- Primary artifacts are `result.txt`, `embedding.json`, `transcript.txt`, or
  `audio.wav`; failures use `error.txt`. Completed embedding JSON includes the
  full vector, so use workspace status rather than job retrieval when only
  counts are needed.
- `nrvnad clean <ws>` previews copied attachments in terminal jobs. Add `--apply`
  to remove eligible files. `--include-kept` and `--include-failed` widen scope.
- Use `wrk --json-schema <file>` for schema-constrained text or vision output.
  Use `wrk --grammar <file>` for existing GBNF. Do not combine these options.
- Structured jobs preserve `schema.json` when provided and always preserve the
  effective `grammar.gbnf`. Invalid JSON from `--json-schema` jobs fails the
  job before publication. `flw --json` reports `output_format`.

Put required prior evidence in the next prompt. Keep lineage separate.

```bash
PARENT=$("$BIN/wrk" "$WS" "Extract the facts: Sales rose 8%. Costs rose 3%.")
"$BIN/nrvnad" "$WS" "$MODEL" --drain
"$BIN/flw" "$WS" "$PARENT" > facts.txt
```

Continue only if submission, execution, and retrieval succeeded. Check `facts.txt`
against the source before submitting the next stage.

```bash
{ echo "Name one unanswered question about these facts:"; cat facts.txt; } \
  | "$BIN/wrk" "$WS" - --parent "$PARENT" --tag followup
"$BIN/nrvnad" "$WS" "$MODEL" --drain
"$BIN/flw" "$WS" --tag followup --json
```

`wrk` stages each submission and publishes it with an atomic rename. Workers
also claim jobs with an atomic rename. The daemon publishes terminal output
under `output/` or `failed/`. After a crash, the next daemon recovers abandoned
`processing/` jobs. The job's `meta.json` carries `recovery_attempts`; a
recovered job can run again until it hits the recovery ceiling, then it lands
in `failed/`. Only one terminal job directory becomes visible. Treat execution
as at least once.

Model names resolve under `./models` or `NRVNA_MODELS_DIR`. `nrvnad` detects
matching mmproj and vocoder files beside the model. llama.cpp estimates GPU
placement when the build supports it. Set `NRVNA_GPU_LAYERS=0` to force CPU.
TTS models and vocoders use CPU placement. Use `nrvnad --help` for job types
and model requirements.

nrvna does not assemble parent context or execute dependency graphs. It does
not route models, parse documents, retry failures, or search artifacts. Callers
own orchestration, context selection, validation, and search.

## Working On This Repository

For ordinary operation, start with this guide and the commands' `--help` output.
Use `INSTALL.md` for model setup, `CONFIGURATION.md` for settings, and
`ADVANCED.md` for composition examples.

When changing code, also follow these rules:

- Read `README.md` for the product contract.
- Read `ARCHITECTURE.md` for ownership and state transitions.
- Treat `include/nrvna/contract.hpp` and `include/nrvna/lifecycle.hpp` as the
  authoritative filesystem and daemon contracts.
- Do not edit `third_party/llama.cpp` casually. Backend updates are isolated,
  reviewed against the upstream examples, and committed as submodule changes.
- Keep stdout machine-readable; diagnostics and logs belong on stderr.
- Before committing core changes, run:

```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## Writing

Use ASD-STE100 Simplified Technical English principles for technical documents
and procedures.

- Use common, precise words.
- Use one term for one concept. Give each term one meaning.
- Keep instructions at 20 words or less.
- Use active voice.
- Write short paragraphs. Put one topic in each paragraph.
- Avoid idioms, filler, hype, and unnecessary jargon.
- Define a necessary technical term before you use it.
- Do not rewrite commands, identifiers, file names, or quoted output.
- Use `nrvna` in lowercase unless a case-sensitive identifier requires another
  form.

Follow Zinsser's four principles. STE gives the mechanics; these give the intent.

- Simplicity: cut every word that does no work.
- Brevity: say it in fewer words. The shorter version is almost always better.
- Clarity: one idea per sentence. The reader must not re-read to understand.
- Humanity: write like a person to a person. Plain is not cold.

Use the same clarity rules for comments, interface text, commits, pull requests,
reports, and agent replies. Do not describe these surfaces as formally
ASD-STE100 compliant.

Product pages and personal writing can use a natural voice. Keep them concise
and unambiguous.

Formal compliance requires the official controlled dictionary and all writing
rules. Do not claim compliance without validated vocabulary and grammar checks.
