# Agent Guide

nrvna provides durable local inference through three commands. `wrk` submits
jobs. `nrvnad` executes them. `flw` reads results. nrvna is not a chat API or
agent framework. Compose it with processes, exit codes, JSON, and files.

Every `wrk` submission creates an independent durable job. Context does not
continue between jobs. `--parent` records lineage only. It does not copy
context, wait for the parent, or control execution order.
The workspace remembers. The model does not.

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

Use the same clarity rules for comments, interface text, commits, pull requests,
reports, and agent replies. Do not describe these surfaces as formally
ASD-STE100 compliant.

Product pages and personal writing can use a natural voice. Keep them concise
and unambiguous.

Formal compliance requires the official controlled dictionary and all writing
rules. Do not claim compliance without validated vocabulary and grammar checks.

## Cold Run

Source builds put binaries under `./build`. Packaged applications put them
under `./bin`. Set `BIN` for the applicable directory. Provide a compatible
GGUF.

```bash
BIN=./build
WS=/tmp/nrvna-agent
MODEL=./models/model.gguf
JOB1=$("$BIN/wrk" "$WS" "Reply with exactly: first" --tag quickstart)
JOB2=$("$BIN/wrk" "$WS" "Reply with exactly: second" --tag quickstart)
"$BIN/flw" "$WS" --json
"$BIN/nrvnad" "$MODEL" "$WS" --workers 2 --drain
"$BIN/flw" "$WS" "$JOB1" --json
"$BIN/flw" "$WS" "$JOB2" --json
```

`wrk` creates the workspace when needed. It prints only the job ID on stdout.
Before the daemon starts, status shows two queued jobs. `--drain` loads the
model and starts the two configured workers. It exits when it observes an idle
queue. A nonzero exit means that the drain failed. Inspect the workspace with
`flw`. No process must stay open.

Use `--drain` for bounded batches, model swapping, and constrained hardware.
For repeated low-latency work, keep one daemon on the workspace:

```bash
"$BIN/nrvnad" "$MODEL" "$WS"
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
- Job inputs become immutable under `input/ready/`. The daemon moves the job
  directory and adds artifacts. Callers must not edit published jobs.
- Failures are terminal and preserved under `failed/`. Retry policy belongs to
  the calling app or agent, not the primitive.
- Primary artifacts are `result.txt`, `embedding.json`, `transcript.txt`, or
  `audio.wav`; failures use `error.txt`. Completed embedding JSON includes the
  full vector, so use workspace status rather than job retrieval when only
  counts are needed.
- Use `wrk --json-schema <file>` for schema-constrained text or vision output.
  Use `wrk --grammar <file>` for existing GBNF. Do not combine these options.
- Structured jobs preserve `schema.json` when provided and always preserve the
  effective `grammar.gbnf`. Invalid JSON from `--json-schema` jobs fails the
  job before publication. `flw --json` reports `output_format`.

Put required prior evidence in the next prompt. Keep lineage separate.

```bash
PARENT=$("$BIN/wrk" "$WS" "Extract the important facts")
"$BIN/nrvnad" "$MODEL" "$WS" --drain
CHILD=$({ echo "Previous result:"; "$BIN/flw" "$WS" "$PARENT"; \
  echo "Convert those facts to JSON."; } \
  | "$BIN/wrk" "$WS" - --parent "$PARENT")
"$BIN/nrvnad" "$MODEL" "$WS" --drain
"$BIN/flw" "$WS" "$CHILD"
```

`wrk` stages each submission and publishes it with an atomic rename. Workers
also claim jobs with an atomic rename. The daemon publishes terminal output
under `output/` or `failed/`. After a crash, the next daemon recovers abandoned
`processing/` jobs. A recovered job can run again. Only one terminal job
directory becomes visible. Treat execution as at least once.

Model names resolve under `./models` or `NRVNA_MODELS_DIR`. `nrvnad` detects
matching mmproj and vocoder files beside the model. Use `nrvnad --help` for job
types and model requirements.

nrvna does not assemble parent context or execute dependency graphs. It does
not route models, parse documents, retry failures, or search artifacts. Callers
own orchestration, context selection, validation, and search.

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
(cd apps/imgsrch && go test ./...)
```
