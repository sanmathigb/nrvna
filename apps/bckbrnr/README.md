# bckbrnr

`bckbrnr` is a macOS menu-bar app for local prompt work. Type a prompt, walk
away, and pick up the answer as a file.

It is a first-party app built on nrvna primitives. It does not call llama.cpp
directly and does not implement a second inference path: the app bundles and
uses `nrvnad`, `wrk`, and `flw`.

## Build

From the repository root, build the primitive binaries first:

```bash
cmake -S . -B build
cmake --build build -j4 --target nrvnad wrk flw
```

Then build and launch the app:

```bash
cd apps/bckbrnr
make run
```

`make app` creates:

```text
.build/bckbrnr.app/
└── Contents/Resources/bin/
    ├── nrvnad
    ├── wrk
    └── flw
```

## Use

1. Open `bckbrnr` from the menu bar.
2. Choose a GGUF text model.
3. Start the text utility.
4. Type a prompt and press Enter.
5. Read the answer from `~/bckbrnr/text/response/`.

The prompt is preserved in:

```text
~/bckbrnr/text/prompt/
```

The hidden nrvna workspace remains inspectable at:

```text
~/bckbrnr/text/.ws/
```

## Durability Contract

The user-facing contract is files in, files out:

- prompts are written to `prompt/`
- completed answers are written to `response/`
- failures are written as artifacts, not only logs
- completed nrvna outputs are backfilled into `response/` if the app restarts

## Status

MVP. Text utility only.

Not yet solved:

- signed and notarized distribution
- model download/install UX
- modern `UserNotifications` replacement for deprecated `NSUserNotification`
- vision, embedding, TTS, and STT utilities
