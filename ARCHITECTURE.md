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
   (+ type.txt for embed/tts, images/ for vision)
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
   e. On success: write output file, RENAME -> output/<job_id>
   f. On failure: write error.txt, RENAME -> failed/<job_id>
```

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

`nrvnad <model> <ws> --drain` processes work until it observes an idle queue.
It then exits. If another daemon owns the workspace, drain waits for that
daemon to finish the queue.

## Inference Pipeline

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

Based on llama.cpp `tools/tts/tts.cpp`.

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
| `nrvnad` | Start daemon | `nrvnad model.gguf workspace` |
| `nrvnad status` | Daemon state | `nrvnad status workspace --json` |
| `nrvnad stop` | Stop a workspace daemon | `nrvnad stop workspace` |
| `nrvnad --drain` | Process queue to quiet, then exit | `nrvnad model.gguf workspace --drain` |
| `wrk` | Submit jobs | `wrk workspace "prompt"` |
| `flw` | Inspect or wait for results | `flw workspace -w job-id` |

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
