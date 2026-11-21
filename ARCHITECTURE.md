# nrvna Architecture

## Overview

nrvna provides durable local inference primitives. A workspace stores jobs in
directories. Atomic renames move each job between states. llama.cpp performs
the model inference.

## Directory Structure

The state directories and artifact names are the public job layout.

```
WORKSPACE/
├── input/
│   ├── writing/      <- Jobs being created (staging area)
│   └── ready/        <- Jobs waiting to be processed
├── processing/       <- Jobs currently running inference
├── output/           <- Completed jobs with results
└── failed/           <- Failed jobs with error messages
```

## Components

| Component | File | Role |
|-----------|------|------|
| **Work** | `work.hpp/cpp` | Validates and submits jobs |
| **Flow** | `flow.hpp/cpp` | Reads job status and results |
| **Server** | `server.hpp/cpp` | Owns the scanner, pool, and processor |
| **Scanner** | `scanner.hpp/cpp` | Finds jobs in `input/ready/` |
| **Pool** | `pool.hpp/cpp` | Runs worker threads |
| **Processor** | `processor.hpp/cpp` | Routes and completes jobs |
| **Runner** | `runner.hpp/cpp` | Runs text, vision, embedding, and speech-to-text inference |
| **TtsRunner** | `runner_tts.hpp/cpp` | Runs OuteTTS and the vocoder |
| **Logger** | `logger.hpp/cpp` | Writes thread-safe logs to stderr |
| **Contract** | `contract.hpp` | Defines job states, IDs, types, and artifacts |

## Workflow: Job Submission (Client Side)

```
1. Client calls Work::submit(prompt)
         |
         v
2. Create directory: input/writing/<job_id>/
         |
         v
3. Write prompt and optional structured-output files under input/writing/<job_id>/
   (+ type.txt for embed/tts; input paths in meta.json by default)
         |
         v
4. ATOMIC RENAME: input/writing/<job_id> -> input/ready/<job_id>
         |
         v
5. Return job_id to client
```

## Workflow: Job Processing (Server Side)

```
SERVER (main thread)
  |
  +-- Scanner Thread (scanLoop)
  |     +-- Every 5s: scan input/ready/ for new jobs
  |     +-- Submit found job IDs to Pool
  |
  +-- Worker Threads (Pool)
        +-- Worker-0, Worker-1, ... Worker-N
        +-- Each pulls jobs from queue
        +-- Each has its own Runner + TtsRunner instance
```

### Per-Job Processing

```
1. Scanner finds job in input/ready/<job_id>
         |
         v
2. Pool assigns to worker thread
         |
         v
3. Processor::process() called:
   a. ATOMIC RENAME: input/ready/<job_id> -> processing/<job_id>
   b. Read prompt and optional grammar from processing/<job_id>/
   c. Read type from processing/<job_id>/type.txt (default: text)
   d. Route by type:
      - text/vision → Runner::run()       → result.txt (optional GBNF constraint)
      - embed       → Runner::embed()     → embedding.json
      - stt (--audio) → Runner::transcribe() → transcript.txt
      - tts         → TtsRunner::run()    → audio.wav
   e. On success: write output file, RENAME -> output/<job_id>. Referenced
      input paths remain in meta.json; copied inputs remain only when requested.
   f. On failure: write error.txt, RENAME -> failed/<job_id>
```

Prompts and metadata remain after success. Image and audio jobs record source
paths in `meta.json` by default. The worker opens those paths when it claims the
job. Callers must keep the files available until completion and recovery.
`wrk --keep-files` copies inputs into the job and keeps those copies after
success. Older jobs keep their existing copied-input behavior. Startup cleanup
and `nrvnad clean` apply only to copied attachments.

## Workflow: Result Retrieval (Client Side)

```
Client calls Flow::status(job_id)
         |
         v
Check directories in order:
  - input/ready/<job_id> -> Status::Queued
  - processing/<job_id>  -> Status::Running
  - output/<job_id>      -> Status::Done
  - failed/<job_id>      -> Status::Failed
  - none found           -> Status::Missing

Client calls Flow::get(job_id)
         |
         v
Read output/<job_id>/result.txt (or error.txt if failed)
```

## Job States

| State | Directory | Description |
|-------|-----------|-------------|
| STAGING | `input/writing/<id>` | Being created, not yet visible |
| QUEUED | `input/ready/<id>` | Waiting for worker |
| RUNNING | `processing/<id>` | Inference in progress |
| DONE | `output/<id>` | Completed successfully |
| FAILED | `failed/<id>` | Error occurred |

## Daemon Lifecycle

`include/nrvna/lifecycle.hpp` defines the daemon lifecycle. The workspace can
contain `.nrvnad.lock`, `.nrvnad.pid`, `.nrvnad.ready`, and `.nrvnad.info`.
A held lock indicates liveness. The `.nrvnad.lock` file can remain after exit.
The ready file appears after the model loads. The info file contains the PID,
model, workers, and start time as JSON.

Use `nrvnad status` to read daemon state. It returns `0` for ready, `2` for
starting, and `1` for not running. Use `nrvnad stop` for a graceful stop.

`nrvnad <ws> <model> --drain` processes work until it observes an idle queue.
It then exits. If another daemon owns the workspace, drain waits for that
daemon to finish the queue.

## Inference Pipeline

### Backend version

The backend submodule is pinned to llama.cpp v1.0.0 at commit `b1b770d90`.
nrvna links the local llama, mtmd, and common libraries. It does not enable the
upstream HTTP server. nrvna uses the backend APIs for model loading, media,
chat templates, and structured output.

Run `bash tests/backend-smoke.sh` with existing models to check text, structured
JSON, and embeddings. Supply a vision model and projector to also check image
input. The script retains temporary artifacts and downloads nothing.

The October 29, 2026 local check passed all ten CTest cases and the backend smoke
checks for text, constrained JSON, and embeddings on Intel macOS. It used
SmolLM2 and nomic-embed. Speech, vision, TTS, GPU inference, and Linux were not
runtime-tested in this check.

### Text/Vision (Runner)

Based on llama.cpp `examples/simple/simple.cpp` and `tools/mtmd/mtmd-cli.cpp`.

- All workers share one thread-safe `llama_model`.
- Each worker creates a new `llama_context` for each job.
- Each worker owns one `mtmd_context`. This context is not thread-safe.
- A mutex serializes vision encoding because GGML shares compute graph state.
- `common_chat_templates` applies the Jinja chat template.
- `NRVNA_CHAT_TEMPLATE_FILE` overrides the model template. An unreadable file
  stops startup.
- The sampler order is penalties, top-k, top-p, min-p, temperature, and
  distribution.
- `stripThinkBlocks()` removes `<think>...</think>` blocks.

### TTS (TtsRunner)

Based on llama.cpp `tools/tts/tts.cpp` at the previous `00fa7cb28` pin.
The current upstream example uses a different audio-generation path. This update
keeps nrvna's existing OuteTTS implementation.

- All workers share the TTS and vocoder models.
- Vocabulary checks detect OuteTTS v0.2 or v0.3.
- Audio code generation uses a top-k value of `4`.
- The runner extracts codes from `<|N|>` token text.
- The vocoder converts codes to embeddings, an ISTFT spectrum, and 24 kHz PCM.

### Embeddings (Runner::embed)

- The runner creates a context with `embeddings=true` and mean pooling.
- It returns a float vector. The model defines the vector dimension.

## Logging

All logs go to stderr. Stdout contains machine-readable command output.

### Log Levels

| Level | Value | Description |
|-------|-------|-------------|
| ERROR | 0 | Errors only |
| WARN | 1 | Warnings and above |
| INFO | 2 | General info (default) |
| DEBUG | 3 | Detailed debugging |
| TRACE | 4 | Very verbose tracing |

### Configuration

```bash
export NRVNA_LOG_LEVEL=debug    # Options: error, warn, info, debug, trace
export LLAMA_LOG_LEVEL=error    # Controls llama.cpp verbosity (default: error)
```

### Log Format

```
[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [ThreadName] Message
```

## CLI Tools

| Tool | Purpose | Example |
|------|---------|---------|
| `nrvnad` | Start daemon | `nrvnad workspace model.gguf` |
| `nrvnad status` | Daemon state | `nrvnad status workspace --json` |
| `nrvnad stop` | Stop a workspace daemon | `nrvnad stop workspace` |
| `nrvnad --drain` | Process queue to quiet, then exit | `nrvnad workspace model.gguf --drain` |
| `wrk` | Submit jobs | `wrk workspace "prompt"` |
| `flw` | Inspect or wait for results | `flw workspace -w job-id` |

## Application and Frontend Contract

Applications may use a GUI, TUI, CLI, watcher, or another intake. They remain
clients of the same three commands: `wrk` submits, `nrvnad` processes, and
`flw` reads.

The workspace is the durable source of job state and results. A frontend may
keep a small display cache, but it must not make that cache the only copy of
progress. Rebuild job status and input mappings from `flw` and job metadata
after restart when the workspace has the required fields. Save only application
state that the job metadata does not contain. Use `nrvnad status` for daemon
state; do not read daemon lifecycle files directly.

Submission returns a job ID before inference completes. A frontend should save
that ID and let the user continue. It may wait when that is the intended task,
but waiting must not own the job's durability. Use the workspace contract to
resume work after the frontend or worker exits.

Treat filesystem notifications as hints. A frontend may watch a workspace to
refresh quickly, then reconcile through `flw`. Do not add a database, broker,
or service only to synchronize frontends unless a real workload requires it.

Completed artifact folders can feed another model role. A recipe reads selected
artifacts, adds the next stage's instructions, and submits new jobs with `wrk`.
The prompt carries the evidence. `--parent` records lineage only. `wrk` must
publish each complete job; callers must not copy loose artifacts into
`input/ready/` or edit published jobs. See [ADVANCED.md](ADVANCED.md) for a
follow-up example.

### Application Design Rules

Start each design choice from the work and its durable files. Do not choose a
queue, server, database, cache, or in-memory catalog because the UI framework
makes it easy. Add one only when a stated workload needs it and its cost is
clear.

Stream input paths in filesystem order. With reference inputs, do not add an
arbitrary submission cap or wait after each group. Let the nrvna worker count
limit active inference. Add submission backpressure only when measurements show
a filesystem or workspace limit. Keep image bytes and full result text out of
collection-wide memory. Keep only the visible review page in memory. Read a
selected result from its nrvna job when needed.

Let the workspace carry job state, prompts, metadata, and results. Rebuild a
frontend view from those files after restart. Do not copy durable job state into
a second catalog unless a measured workload requires an index. If an index is
needed, name it as a rebuildable index and document its source of truth.

An app may add workflow rules, prompts, input discovery, and review. It must
state which layer owns each file and transition. Keep each job's prompt and
input evidence explicit. A later job receives prior evidence in its prompt;
parent lineage does not provide context.

Every added structure needs a workload reason, a memory or disk bound, a
recovery rule, and one owner. A language feature is not a system design reason.

Before adding a queue, retry policy, index, cache, or model router, check
whether the workspace, job states, IDs, and tags already express the need. Add
a shared core mechanism only when more than one caller needs it. Keep caller
policy in the application. Do not import a synchronous service pattern only
because it is familiar or available.

Generated job IDs remain the default for independent submissions from different
terminals or devices. A caller-selected stable ID is an optional way to address
the same logical work again. It does not replace generated IDs. Tags group jobs;
parent IDs record lineage; neither provides stable work identity. `--drain` and
a persistent daemon are execution modes over the same workspace, not different
job identity or recovery models. Use separate workspaces for model roles, and
run those roles concurrently only when the hardware can support them.

### Caller-Addressable Jobs

Execution resume and application-view resume are separate. The daemon resumes
jobs from `processing/` and `input/ready/`. An application that rebuilds a view
must also reconnect each application item to its job.

`wrk --id <id>` accepts a caller-chosen stable job ID. The ID is the job
directory name and the address. The caller can derive it again after restart.
Without `--id`, `wrk` keeps generating IDs as before.

The ID must be one safe lowercase filesystem component with a letter. nrvna
serializes caller-ID publication and returns an existing job unchanged. The
caller owns the ID recipe and must not reuse an ID for different work. nrvna
does not inspect inputs or compare prompts to check that recipe.
The lock file is `input/.id-submit.lock`. It coordinates publication; it is
not a job record or lookup index. Jobs stage under temporary IDs, then publish
under the caller ID while holding this lock.

The ID names one unit of work, not only one input. It must distinguish the
input version and operation, such as OCR versus Vision. Include the effective
prompt and model revision unless the workspace fixes them. The caller owns this
identity policy. Do not infer it from tags; tags group jobs, but do not identify
one job.

For file inputs, the caller can include a content fingerprint. Path plus size
and modification time is a fast change fingerprint, not proof that the bytes
are unchanged. A content hash gives a stronger identity but reads the full
file. The application must choose and document the guarantee it needs.

Submission with an existing ID returns that job, whether queued, running, done,
or failed. A repeated submission does not replace its prompt or metadata. A
deliberate retry uses a new ID. This preserves terminal job records and leaves
retry policy with the caller.

An application can derive IDs for its current items and check the known state
directories without enumerating all `M` jobs. Reconciliation takes `O(N)` job
lookups and bounded application memory, plus the cost of computing each input
fingerprint. It does not list orphaned jobs whose source items are gone; a
separate workspace cleanup operation owns that work.

This contract applies across modalities. It makes submission idempotent; it
does not promise exactly-once inference. A recovered job can still run again.

Prove small-run inference quality before scale work. Then find count-driven
limits with many small, representative files. Measure scan time, submission
rate, memory, and workspace growth before adding backpressure. Do not stage a
large real archive to find a limit that a small synthetic workspace can expose.

## Key Design Decisions

1. **Use atomic renames.** POSIX directory renames provide one winner without
   a database lock.
2. **Use the directory as state.** The job's location defines its state.
3. **Share model weights.** Each worker uses its own inference context.
4. **Keep state in files.** Jobs survive process failure and remain readable.
5. **Complete every claim.** If `finalizeSuccess` fails, the processor calls
   `finalizeFailure`.

## Environment Variables

[CONFIGURATION.md](CONFIGURATION.md) contains the authoritative variable list.
It covers runtime, sampling, media, safety, logging, discovery, and application
settings. Do not copy defaults into this document.

## Thread Model

```
Main Thread
    |
    +-- creates Server
    |       |
    |       +-- creates Scanner (1 thread)
    |       +-- creates Pool (N worker threads)
    |       +-- creates Processor (shared, thread-safe)
    |       |       +-- pre-initializes N Runners
    |       |       +-- pre-initializes N TtsRunners (if vocoder present)
    |       |
    |       +-- recoverOrphanedJobs (processing/ -> ready/ or failed/ at the
    |                                  recovery ceiling)
    |
    +-- waits for shutdown signal (SIGINT/SIGTERM)

Scanner Thread
    +-- loops every 5 seconds
    +-- scans input/ready/
    +-- submits jobs to Pool queue

Worker Threads (N)
    +-- wait on condition variable
    +-- pop job from queue
    +-- call Processor::process(job_id, worker_id)
    +-- each has dedicated Runner + TtsRunner instance
```
