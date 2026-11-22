# Agent Guide

nrvna provides durable local inference through three commands. `wrk` submits
jobs. `nrvnad` executes them. `flw` reads results. nrvna is not a chat API or
agent framework. Compose it with processes, exit codes, JSON, and files.

Every new `wrk` submission creates an independent durable job. A repeated
`--id` returns the existing job. Context does not continue between jobs.
`--parent` records lineage only. It does not copy context, wait for the parent,
or control execution order.

## What nrvna Gives You

nrvna gives you six properties. Use each one on purpose. Do not rebuild what nrvna already provides.

### Work and results are files
A job is a directory. The prompt, state, result, and metadata are files. The state machine is a set of directory moves: ready, then processing, then output or failed. There is no server, database, or network API.
- Use it: compose nrvna with any language or tool that reads and writes files. Read state with `ls`, `cat`, and `jq`. Give a job a stable caller ID that you derive from your input, so you can find the job again after a restart.
- Avoid: a second database or index that holds the same state. The files are the state.

### Work survives a crash
nrvna accepts a job as a durable record. A process crash does not lose accepted work. The next daemon re-runs an interrupted job from the start, up to a recovery ceiling.
- Use it: submit large or long batches, then leave. Submit work before any model loads.
- Avoid: your own retry or checkpoint logic. nrvna recovers abandoned jobs.
- Limit: nrvna tests process-crash recovery, not power loss. Execution is at least once, not exactly once.

### Work and model run at different times
`wrk` submits with no daemon running. `nrvnad` processes the work later. `flw` reads results at any time. The model loads and frees around the work.
- Use it: submit now, run later. Drain a batch so the model loads one time. Run models in sequence when memory is tight.
- Avoid: one hot model that handles one job at a time while a person watches. That use removes the batch advantage.

### Many models, one pipeline
One daemon runs one model against one workspace. Chain stages through files: one stage writes a result, the next stage reads it.
- Use it: build a pipeline such as caption, then OCR, then embed. Swap a model by pointing a daemon at a different GGUF file. Pass evidence between stages in the next prompt.
- Avoid: hidden context between stages. Each prompt must carry the evidence that the job needs.

### Small and swappable
nrvna runs models. Storage, search, user interface, and orchestration are not part of nrvna. They connect through the file contract.
- Use it: keep the models in nrvna. Send vectors to a store such as sqlite-vec. Replace the store, the model, or the caller without a change to nrvna.
- Avoid: a vector database or a workflow engine inside nrvna. The file contract is the boundary.

### Independent, non-real-time jobs
Jobs do not depend on each other. No one watches a job live. Each job runs in a fresh context.
- Use it: reorder, group, and batch jobs freely. Size memory for each job.
- Avoid: ordering, sessions, or streaming that the work does not need.

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

nrvna runs any GGUF model that llama.cpp supports. You choose and download the model. nrvna does not install or manage models.

To find a model:

1. Open the Hugging Face model list.
2. Filter by GGUF format.
3. Filter by llama.cpp compatibility.
4. Enter your hardware. Choose a quantization that fits your memory.

To match a job type:

- A text job needs one model file.
- A vision job needs a model file and its mmproj projector file.
- A speech-to-text job needs a model file and its projector file.
- A text-to-speech job needs a model file and a vocoder file.

Put the files where `nrvnad` resolves them: `./models` or `NRVNA_MODELS_DIR`. `nrvnad` finds a matching projector or vocoder beside the model.

Read a model's card before its first real job. Apply its template and recommended generation settings. nrvna reads supported GGUF sampling hints, not model-card prose, so set the values that you need. Use a low temperature for extraction and other exact tasks. Set `NRVNA_GPU_LAYERS=0` to force CPU.

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

Source builds put binaries under `./build`. Packaged binaries go under `./bin`.
Set `BIN` for the applicable directory. Provide a compatible
GGUF.

```bash
BIN=./build
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
queue. A nonzero exit means that the drain failed. Inspect the workspace with
`flw`. No process must stay open.

Use `--drain` for bounded batches, model swapping, and constrained hardware.
For repeated low-latency work, start a daemon in one terminal:

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
PARENT=$("$BIN/wrk" "$WS" "Extract the important facts")
"$BIN/nrvnad" "$WS" "$MODEL" --drain
CHILD=$({ echo "Previous result:"; "$BIN/flw" "$WS" "$PARENT"; \
  echo "Convert those facts to JSON."; } \
  | "$BIN/wrk" "$WS" - --parent "$PARENT")
"$BIN/nrvnad" "$WS" "$MODEL" --drain
"$BIN/flw" "$WS" "$CHILD"
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

## Built on nrvna

nrvna is a substrate. This release ships the primitives only: `wrk`, `nrvnad`, and `flw`.

People build applications on top of the same three commands and the same file contract. nrvna is open to new work built on the contract. The project announces applications with its releases, not in this guide, so this guide stays stable as applications appear.

To build an application, follow "Build Applications From the Same Contract" and its test: use what nrvna already provides, and add only the logic that your product needs.

## Working On This Repository

For ordinary operation, this guide and the three commands' `--help` output are
enough. Read the implementation documents below only when changing the code.

- Read `README.md` for the product contract.
- Read `ARCHITECTURE.md` for ownership and state transitions.
- Read `ADVANCED.md` for composition patterns.
- Read `CONFIGURATION.md` for runtime settings.
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
