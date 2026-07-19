# bckbrnr

Type a prompt. Walk away. The answer is a file.

`bckbrnr` is a macOS menu-bar app for local prompt work. It runs a GGUF model
on your Mac and writes every answer to a folder. No account, server, or chat
session is required.

## Install

1. Download `bckbrnr-darwin-arm64.zip` for Apple Silicon or
   `bckbrnr-darwin-x86_64.zip` for an Intel Mac from the
   [latest release](https://github.com/sanmathigb/nrvna-ai/releases/latest).
2. Unzip it and move `bckbrnr.app` to Applications.
3. Right-click the app and choose **Open** on first launch.

The developer preview is unsigned and not notarized, so a normal double-click
may be blocked the first time. It requires macOS 13.3 or newer.

## First Prompt

1. Open the menu-bar circle.
2. Choose an instruct GGUF model. For a small starting point, download
   [`LFM2.5-1.2B-Instruct-Q4_K_M.gguf`](https://huggingface.co/LiquidAI/LFM2.5-1.2B-Instruct-GGUF/blob/main/LFM2.5-1.2B-Instruct-Q4_K_M.gguf).
3. Press **Start** and wait for **Ready**.
4. Type a prompt and press Return.
5. Leave. A notification opens the answer when it is ready.

Click the folder path in the popover at any time to open your answers:

```text
~/bckbrnr/text/response/
```

The app bundles `nrvnad`, `wrk`, and `flw`. The model is the only separate
download. Inference, prompts, and answers stay on this Mac.

## Files Are the Contract

```text
~/bckbrnr/text/
├── response/          answers, readable failures, and the engine log
├── .prompt/           copies of submitted prompts
└── .ws/               durable nrvna jobs
```

- answers are written to `response/<prompt>.txt`
- failures are written to `response/<prompt>.error.txt`
- rapid prompts are submitted durably before earlier answers finish
- completed answers are recovered when the app reopens, even without a daemon
- unfinished jobs remain in `.ws/` and resume the next time you press Start

You may change the answer folder while the utility is stopped. The app never
uploads or deletes your model, prompts, jobs, or answers.

## Build From Source

From the repository root:

```bash
cd apps/bckbrnr
make test
make app
open .build/bckbrnr.app
```

`make app` builds portable, statically linked nrvna engine binaries and bundles
them under `Contents/Resources/bin/`. Use `make run` to build and open in one
command. Maintainers can run the model-backed controller journey with
`make integration-test MODEL=/path/to/model.gguf`.

## Status

Developer preview. Text prompts only. Apple Silicon and Intel releases are
built and smoke-tested on macOS; signed and notarized distribution remains to
be done.
