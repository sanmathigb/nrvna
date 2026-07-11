# nrvna-ai Domain Language

Terms used consistently across code, docs, and reviews. If code and this
file disagree, one of them has a bug.

- **Primitive** — one of the three commands: `nrvnad` (daemon), `wrk`
  (submit), `flw` (collect). Small on purpose; everything else builds on top.
- **Workspace** — a directory that is a complete, self-describing job queue.
  Contains the five state directories. Owned by at most one running `nrvnad`.
- **Job** — a directory. Its location is its state; its files are its data.
  Never partially visible: staged in `input/writing/`, published by one
  atomic rename.
- **State** — which state directory a job lives in: Queued (`input/ready/`),
  Running (`processing/`), Done (`output/`), Failed (`failed/`). Missing =
  no directory anywhere.
- **Claim** — the atomic rename `input/ready/<id>` → `processing/<id>`.
  The only mechanism that assigns a job to a worker; POSIX guarantees one winner.
- **Artifact** — the primary output file of a Done job. Exactly one per job,
  resolved by priority: `result.txt` > `transcript.txt` > `audio.wav` >
  `embedding.json`.
- **Job contract** — the full on-disk vocabulary: state directories, artifact
  filenames, the artifact rule, job-ID grammar, `type.txt` spellings. Single
  owner: `include/nrvna/contract.hpp`. Non-C++ consumers cross the contract
  via `wrk`/`flw` (`flw --json` carries `artifact_kind`/`artifact_path`),
  never by touching the layout directly. (imgsrch's collect path and
  bckbrnr predate this rule; their migrations are queued.)
- **Language policy** — below the CLI seam: C++17 only (the engine never gains
  a second language). Above it: each surface uses its native language (Go for
  portable CLI apps, Swift for macOS GUI, bash for glue), and every language
  crosses into nrvna ONLY via the three binaries and the documented contracts —
  no FFI, no bindings, no workspace-layout reading. Python is never a runtime
  dependency of anything tracked.
- **Lifecycle contract** — the daemon-management files (`.nrvnad.lock`,
  `.nrvnad.pid`, `.nrvnad.ready`, `.nrvnad.info`). Deliberately NOT part of
  the job contract. Single owner: nrvnad (`include/nrvna/lifecycle.hpp`);
  `nrvnad status`/`stop` are the blessed readers, `--drain` the run-to-done mode.
