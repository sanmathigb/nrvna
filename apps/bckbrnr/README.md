# bckbrnr

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/sanmathigb/nrvna/blob/main/LICENSE)
[![Built with nrvna](https://img.shields.io/badge/built_with-nrvna-5b5bd6.svg)](https://github.com/sanmathigb/nrvna)

Type a prompt. Walk away. The answer is a file.

`bckbrnr` is a macOS menu-bar app for local prompt work. It runs a GGUF model
on your Mac. You can submit more work while earlier prompts run. The app writes
each answer to a folder.

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

Unzip the archive. Move `bckbrnr.app` to Applications. Right-click the app and
choose **Open** on the first launch.

The developer preview has an ad-hoc signature. It is not notarized. macOS can
block a normal double-click on the first launch. The app requires macOS 13.3
or newer.

## Run your first prompt

1. Open the menu-bar circle.
2. Choose an instruct GGUF model. A small starting point is
   [`LFM2.5-1.2B-Instruct-Q4_K_M.gguf`](https://huggingface.co/LiquidAI/LFM2.5-1.2B-Instruct-GGUF/blob/main/LFM2.5-1.2B-Instruct-Q4_K_M.gguf).
3. Press **Start** and wait for **Ready**.
4. Type a prompt and press Return.
5. Leave the app. A notification opens the answer when it is ready.

The model is the only separate download. The app includes `nrvnad`, `wrk`, and
`flw`. Inference, prompts, jobs, and answers stay on this Mac.

The first launch asks for permission to send notifications. Grant permission
to receive the ready notice. The app writes answers without this permission.
**Help** in the popover opens this guide.

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

- Answers use `response/<prompt-stem>.txt`.
- Failures use `response/<prompt-stem>.error.txt`.
- The stem comes from the first non-empty prompt line. It is cleaned and capped
  at 40 characters. A numeric suffix prevents collisions.
- Prompts become durable before earlier answers finish.
- The app recovers completed answers when it opens.
- The app resumes unfinished jobs when you press **Start**.

You can change the utility root while the utility is stopped. This changes the
folders for prompts, jobs, and answers. The app does not upload, move, or delete
your model, prompts, jobs, or answers.

## Human app, reusable primitives

bckbrnr provides a menu-bar prompt box, notifications, and an answer folder.
Scripts and agents should use
[nrvna's three CLI primitives](https://github.com/sanmathigb/nrvna) directly.
They should not automate the app interface.

This separation is intentional. bckbrnr owns the product experience. nrvna
owns the durable local work.

## Build from source

From the repository root:

```bash
cd apps/bckbrnr
make test
make app
open .build/bckbrnr.app
```

`make app` builds static nrvna engine binaries. It puts them under
`Contents/Resources/bin/`. Use `make run` to build and open the app. Maintainers
can run the model-backed controller test with:

```bash
make integration-test MODEL=/path/to/model.gguf
```

Engine, model, and runtime environment overrides are listed in the
[nrvna configuration reference](https://github.com/sanmathigb/nrvna/blob/main/CONFIGURATION.md).

## Limits

bckbrnr processes independent text prompts. It does not provide chat history,
token streaming, vision, or tool use. It requires macOS 13.3+ and a local
instruct GGUF. Tests cover Apple Silicon and Intel archives. Distribution is
ad-hoc signed but not Developer ID signed or notarized.

MIT licensed. Model licenses remain model-specific.
