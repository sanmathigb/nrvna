# Documentation

nrvna provides three Unix-like primitives for durable, asynchronous local
inference. `wrk` submits work. `nrvnad` runs a model against a workspace. `flw`
reads status and results.

## Start

- [README](README.md): what nrvna is, one verified run, and its boundaries
- [Quickstart](QUICKSTART.md): build, submit, drain, and retrieve
- [Agent guide](AGENTS.md): machine-readable contracts and a cold-agent run

## Understand

- [Domain language](CONTEXT.md): job, workspace, state, artifact, and lifecycle
- [Advanced patterns](ADVANCED.md): batches, chaining, and multiple models
- [Configuration](CONFIGURATION.md): runtime and application settings
- [Architecture](ARCHITECTURE.md): implementation ownership and state transitions

## Applications

- [imgsrch](apps/imgsrch/README.md): search screenshots by visible words and meaning
- [bckbrnr](apps/bckbrnr/README.md): local prompt work from the macOS menu bar

## Validate

- [Cold-agent test](COLD_AGENT_TEST.md): compare discovery, hands-on behavior,
  and closed-book transfer across agent harnesses without repeating downloads

## Source of truth

The CLI is the public API. Use each command's `--help` output for syntax. Use
`AGENTS.md` for stdout, JSON, exit codes, lifecycle, and recovery. Two headers
define the implementation contracts: `include/nrvna/contract.hpp` and
`include/nrvna/lifecycle.hpp`.

The ignored `docs/` directory contains project notebooks. These files record
research, experiments, workbooks, and plans. They are not the current product
contract.
