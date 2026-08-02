# Agent Guide

nrvna provides durable local inference through three command-line primitives:
`wrk` submits jobs, `nrvnad` executes them, and `flw` reads results. It is not
a chat API or an agent framework. Compose it with processes, exit codes, JSON,
and files.

Every `wrk` submission is an independent durable task. Context does not carry
between jobs. `--parent` records lineage only: it does not copy context, wait
for the parent, or impose execution order. The workspace remembers; the model
does not.

## Cold Run

Source builds place binaries under `./build`; packaged apps place them under
`./bin`. Set `BIN` accordingly and provide a compatible GGUF:

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

`wrk` creates the workspace when needed and prints only the job ID on stdout.
Before the daemon starts, status shows two queued jobs. `--drain` loads the
model, uses the two workers configured in this example, and exits at observed
idle. A nonzero drain exit means the run did not complete cleanly; inspect the
workspace with `flw`. Nothing needs to remain running.

Use `--drain` for bounded batches, model swapping, and constrained hardware.
For repeated low-latency work, keep one daemon on the workspace:

```bash
"$BIN/nrvnad" "$MODEL" "$WS"
JOB=$("$BIN/wrk" "$WS" "task")
"$BIN/flw" "$WS" -w "$JOB"
"$BIN/nrvnad" stop "$WS"
```

Use separate workspaces for different model roles. Drain them sequentially
when their models cannot coexist in memory.

## Machine Contract

- Prefer `--json` when consuming status or results programmatically.
- `flw <ws> <job>` exits `0` when done, `1` when failed or missing, and `2`
  when queued or running.
- `nrvnad status <ws>` exits `0` when ready, `2` while starting, and `1` when
  not running. `nrvnad stop <ws>` exits `0` when stopped or already absent.
- Use `flw <ws> -w <job>` to wait for one job handled by a persistent daemon.
- Use a repeated `--tag <name>` on `wrk`, then `flw <ws> -W --tag <name>` as a
  barrier for that batch. The barrier prints no selected results. Collect them
  separately with `flw <ws> --tag <name> --json`, which emits NDJSON and exits
  `1` if any selected job failed.
- Use `wrk --parent <job-id>` for lineage and `flw --children <job-id>` to read
  direct descendants.
- One daemon owns a workspace at a time. `nrvnad status <ws> --json` is the
  liveness check; `nrvnad stop <ws>` is the graceful stop operation.
- Some agent sandboxes prevent a later tool invocation from signaling a
  process started by an earlier invocation. Prefer `--drain` there. If `stop`
  reports that the daemon still holds the workspace, stop it from its owning
  terminal rather than editing lifecycle files.
- A `.nrvnad.lock` file may remain after exit. Its existence is not liveness;
  do not inspect lifecycle files instead of using `nrvnad status`.
- Job inputs become immutable when published under `input/ready/`. The daemon
  moves the directory and adds terminal artifacts; callers must not edit or
  move published job directories themselves.
- Failures are terminal and preserved under `failed/`. Retry policy belongs to
  the calling app or agent, not the primitive.
- Primary artifacts are `result.txt`, `embedding.json`, `transcript.txt`, or
  `audio.wav`; failures use `error.txt`. Completed embedding JSON includes the
  full vector, so use workspace status rather than job retrieval when only
  counts are needed.

To pass prior evidence forward, put it in the next prompt explicitly. Keep
lineage separate:

```bash
PARENT=$("$BIN/wrk" "$WS" "Extract the important facts")
"$BIN/nrvnad" "$MODEL" "$WS" --drain
CHILD=$({ echo "Previous result:"; "$BIN/flw" "$WS" "$PARENT"; \
  echo "Convert those facts to JSON."; } \
  | "$BIN/wrk" "$WS" - --parent "$PARENT")
"$BIN/nrvnad" "$MODEL" "$WS" --drain
"$BIN/flw" "$WS" "$CHILD"
```

Submission is staged and atomically published. Workers claim by atomic rename;
terminal output is atomically published under `output/` or `failed/`. After a
daemon crash, the next daemon recovers abandoned `processing/` jobs. Compute
may therefore run more than once, but only one terminal job directory is
published. Treat execution as at least once.

Model names resolve under `./models` or `NRVNA_MODELS_DIR`. Matching mmproj and
vocoder files beside the model are auto-detected. Use `nrvnad --help` for job
types and model requirements.

nrvna does not assemble parent context, execute DAGs, route models, parse
documents, retry failures automatically, or search artifacts semantically.
Callers own orchestration, context selection, validation, and search.

## Working On This Repository

For ordinary operation, this guide and the three commands' `--help` output are
enough. Read the implementation documents below only when changing the code.

- Read `README.md` for the product contract, `ARCHITECTURE.md` for ownership and
  state transitions, `ADVANCED.md` for composition patterns, and
  `CONFIGURATION.md` for runtime settings.
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
