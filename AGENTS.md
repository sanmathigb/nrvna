# Agent Guide

nrvna provides durable local inference through three command-line primitives:
`wrk` submits jobs, `nrvnad` executes them, and `flw` reads results. It is not
a chat API or an agent framework. Compose it with processes, exit codes, JSON,
and files.

## Cold Run

Assume the binaries are under `./build` and a compatible GGUF already exists:

```bash
WS=/tmp/nrvna-agent
MODEL=./models/model.gguf
JOB=$(./build/wrk "$WS" "Summarize this in three bullets")
./build/nrvnad "$MODEL" "$WS" --drain
./build/flw "$WS" "$JOB" --json
```

`wrk` creates the workspace when needed and prints only the job ID on stdout.
`--drain` loads the model, processes the queue to an observed idle state, and
exits. A nonzero drain exit means the run did not complete cleanly; inspect the
workspace with `flw`. Nothing needs to remain running.

## Machine Contract

- Prefer `--json` when consuming status or results programmatically.
- `flw <ws> <job>` exits `0` when done, `1` when failed, and `2` when not ready.
- Use `flw <ws> -w <job>` to wait for one job handled by a persistent daemon.
- Use a repeated `--tag <name>` on `wrk`, then `flw <ws> -W --tag <name>` as a
  barrier for that batch.
- Use `wrk --parent <job-id>` for lineage and `flw --children <job-id>` to read
  direct descendants.
- One daemon owns a workspace at a time. `nrvnad status <ws> --json` is the
  liveness check; `nrvnad stop <ws>` is the graceful stop operation.
- A `.nrvnad.lock` file may remain after exit. Its existence is not liveness;
  do not inspect lifecycle files instead of using `nrvnad status`.
- Jobs become immutable when published under `input/ready/`. Do not edit or
  move published job directories yourself.
- Failures are terminal and preserved under `failed/`. Retry policy belongs to
  the calling app or agent, not the primitive.

Model names resolve under `./models` or `NRVNA_MODELS_DIR`. Matching mmproj and
vocoder files beside the model are auto-detected. Use `nrvnad --help` for job
types and model requirements.

## Working On This Repository

For ordinary operation, this guide and the three commands' `--help` output are
enough. Read the implementation documents below only when changing the code.

- Read `README.md` for the product contract, `ARCHITECTURE.md` for ownership and
  state transitions, and `ADVANCED.md` for composition patterns.
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
