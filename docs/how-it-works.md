# How nrvna works

nrvna is task-agnostic and mode-agnostic. It stores independent text, image,
audio, and embedding jobs in a workspace. Each job starts with fresh context.
The workspace is the source of truth for job inputs, state, and results. Use a
separate workspace for each model. Include saved results in later jobs when
needed. A model process can stop without deleting workspace data.

## Submit work

`wrk` writes a job and returns its ID. It does not wait for inference. Keep
submitting as new work arrives. You can submit jobs before starting a model.

```sh
wrk ./writing "List the action items: Priya ships the docs Wednesday and Raj migrates billing."
wrk ./writing - < notes.txt
```

Each job starts with fresh context. nrvna does not add earlier jobs to a later
prompt. To use an earlier result, read it and include the needed evidence in
the next prompt. `--parent` records lineage only.

## Run the jobs

`nrvnad` loads a model and claims jobs from one workspace. Use `--drain` for a
bounded batch. The command exits when the queue becomes idle.

```sh
nrvnad ./writing ./models/smollm2-1.7b.gguf --drain
```

For repeated work, run one daemon for the workspace. Check its state with
`nrvnad status`. Use `nrvnad stop` to stop it cleanly.

## Read results

`flw` reads job state or output. A completed text job has `result.txt`. Other
job types can produce `embedding.json`, `transcript.txt`, or `audio.wav`.

```sh
flw ./writing
```

The workspace stores jobs in state folders:

```text
input/ready/  →  processing/  →  output/
                             └→ failed/
```

Workers claim jobs and publish results with atomic directory renames. Do not
edit a job after `wrk` publishes it under `input/ready/`.

## Work types

| Work | Submit with | Result |
| --- | --- | --- |
| Text generation | prompt or stdin | `result.txt` |
| Text embedding | prompt with `--embed` | `embedding.json` |
| Image analysis | prompt with `--image` | `result.txt` |
| Image embedding | `--embed` with `--image` | `embedding.json` |
| Transcription | `--audio` | `transcript.txt` |
| Speech generation | prompt with `--tts` | `audio.wav` |

Use a model that supports the requested work. Vision and transcription need a
matching projector. Speech generation requirements depend on the model. See
[Configuration](../CONFIGURATION.md#vision-speech-and-media).

Image and audio inputs reference source files by default. Keep those files
available and unchanged until jobs finish, including recovery. Use
`wrk --keep-files` to copy and retain inputs within the job.

`nrvnad clean <workspace>` previews copied attachments eligible for removal.
It does not remove referenced source files. See [AGENTS.md](../AGENTS.md) for
retention and cleanup rules. Check installed command help for options in your
release.

nrvna does not accept video or mixed image-and-audio jobs.

## Recovery

After a daemon crash, the next daemon can recover a job left in `processing/`.
It starts that job again from its saved input. Generated tokens do not resume.
Execution is at least once, so a job can run more than once after a crash.
Recovery stops after a fixed number of attempts. Failed jobs remain available
for inspection; the caller decides whether to submit a new job.

This recovery behavior covers process crashes. It is not a guarantee against
power loss or storage failure.

## What callers build

nrvna does not parse documents, search files, pass context between jobs, or
choose which model runs next. A caller can submit work, inspect results, and
decide what to do next. The caller owns that workflow.

See [Advanced patterns](../ADVANCED.md) for batch and pipeline examples.
