# bckbrnr

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/sanmathigb/nrvna/blob/main/LICENSE)
[![Built with nrvna](https://img.shields.io/badge/built_with-nrvna-5b5bd6.svg)](https://github.com/sanmathigb/nrvna)

Type a prompt. Walk away. The answer is a file.

`bckbrnr` is a macOS menu-bar app for local prompt work. It runs a GGUF model
on your Mac, accepts more work while earlier prompts are running, and writes
every answer to a folder.

```text
prompt  ->  local model  ->  notification  ->  answer.txt
```

No account, server, or chat session is required.

**Developer preview.** Built with
[nrvna](https://github.com/sanmathigb/nrvna).

## Install

Download the archive for your Mac from the
[latest release](https://github.com/sanmathigb/nrvna/releases/latest):

| Mac | Archive |
| --- | --- |
| Apple Silicon | `bckbrnr-darwin-arm64.zip` |
| Intel | `bckbrnr-darwin-x86_64.zip` |

Unzip it, move `bckbrnr.app` to Applications, then right-click the app and
choose **Open** on first launch.

The developer preview is ad-hoc signed but not notarized, so macOS may block a
normal double-click the first time. It requires macOS 13.3 or newer.

## Run your first prompt

1. Open the menu-bar circle.
2. Choose an instruct GGUF model. A small starting point is
   [`LFM2.5-1.2B-Instruct-Q4_K_M.gguf`](https://huggingface.co/LiquidAI/LFM2.5-1.2B-Instruct-GGUF/blob/main/LFM2.5-1.2B-Instruct-Q4_K_M.gguf).
3. Press **Start** and wait for **Ready**.
4. Type a prompt and press Return.
5. Leave. A notification opens the answer when it is ready.

The model is the only separate download. The app bundles the `nrvnad`, `wrk`,
and `flw` engine binaries. Inference, prompts, jobs, and answers stay on this
Mac.

The first launch asks permission to send notifications. Grant it to receive
the ready banner; answers are written to disk either way. **Help** in the
popover opens this guide.

## Answers are files

Click the folder path in the popover to open:

```text
~/bckbrnr/text/response/
```

The complete local state is:

```text
~/bckbrnr/text/
├── response/          answers, readable failures, and the engine log
├── .prompt/           copies of submitted prompts
└── .ws/               durable nrvna jobs
```

- answers are `response/<prompt>.txt`;
- failures are `response/<prompt>.error.txt`;
- prompts are durable before earlier answers finish;
- completed answers are recovered when the app reopens;
- unfinished jobs resume the next time you press **Start**.

You may change the answer folder while the utility is stopped. The app never
uploads or deletes your model, prompts, jobs, or answers.

## Human app, reusable primitives

bckbrnr is the human interface: a menu-bar prompt box, notifications, and an
answer folder. Scripts and agents should use
[nrvna's three CLI primitives](https://github.com/sanmathigb/nrvna) directly
rather than automate the app's UI.

That separation is deliberate. bckbrnr owns the product experience; nrvna
owns the durable local work underneath it.

## Build from source

From the repository root:

```bash
cd apps/bckbrnr
make test
make app
open .build/bckbrnr.app
```

`make app` builds portable, statically linked nrvna engine binaries and
bundles them under `Contents/Resources/bin/`. Use `make run` to build and open
in one command. Maintainers can run the model-backed controller journey with:

```bash
make integration-test MODEL=/path/to/model.gguf
```

Engine, model, and runtime environment overrides are listed in the
[nrvna configuration reference](https://github.com/sanmathigb/nrvna/blob/main/CONFIGURATION.md).

## Limits

bckbrnr is for independent text prompts, not chat history, token streaming,
vision, or tool use. It requires macOS 13.3+ and a local instruct GGUF. Apple
Silicon and Intel archives are built and smoke-tested; signed and notarized
distribution remains to be done.

MIT licensed. Model licenses remain model-specific.
