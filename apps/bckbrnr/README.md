# bckbrnr

`bckbrnr` is a macOS menu-bar app for local prompt work. Type a prompt, walk
away, and pick up the answer as a file.

It is a first-party app built on nrvna primitives. It does not call llama.cpp
directly and does not implement a second inference path: the app bundles and
uses `nrvnad`, `wrk`, and `flw`.

## Build

Build and launch the app:

```bash
cd apps/bckbrnr
make run          # builds the .app and opens it (use `make app` to build only)
make test         # runs the naming tests (no engine build required)
```

The build creates statically linked primitive binaries and bundles them inside
the app, so it does not depend on llama.cpp dylibs from the repository build:

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

The prompt is kept in a hidden sibling folder (you normally only look at `response/`):

```text
~/bckbrnr/text/.prompt/
```

The hidden nrvna workspace remains inspectable at:

```text
~/bckbrnr/text/.ws/
```

## Durability Contract

The user-facing contract is: type a prompt, collect a file.

- prompt copies are kept in hidden `.prompt/`; you normally only see `response/`
- completed answers are written to `response/<stem>.txt`
- failures are written to `response/<stem>.error.txt` — a durable artifact, not just a log
- on startup (and when the popover opens to a live daemon), completed and failed nrvna outputs are reconciled into `response/`, so an answer surfaces even if bckbrnr wasn't running when the job finished

## Status

MVP. Text utility only.

Not yet solved:

- signed and notarized distribution
- model download/install UX
- modern `UserNotifications` replacement for deprecated `NSUserNotification`
- vision, embedding, TTS, and STT utilities
