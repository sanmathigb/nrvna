# Setting up imgsrch (for an AI agent)

You are an AI coding agent setting up `imgsrch` for the person you are helping.
Run the steps yourself, verify each step, and ask only where this guide says
to. Everything runs locally; do not ask for an account, API key, or secret.

## 1. Install

Detect the platform with `uname -sm`, then download the matching archive from
the [latest release](https://github.com/sanmathigb/nrvna/releases/latest):

- `Darwin arm64`: `imgsrch-darwin-arm64.tar.gz`
- `Darwin x86_64`: `imgsrch-darwin-x86_64.tar.gz`
- `Linux x86_64`: `imgsrch-linux-x86_64.tar.gz`

```bash
mkdir -p ~/imgsrch-app
cd ~/imgsrch-app

case "$(uname -sm)" in
  "Darwin arm64")  kit=imgsrch-darwin-arm64 ;;
  "Darwin x86_64") kit=imgsrch-darwin-x86_64 ;;
  "Linux x86_64")  kit=imgsrch-linux-x86_64 ;;
  *)
    echo "imgsrch: unsupported platform: $(uname -sm)" >&2
    exit 1
    ;;
esac

curl -fL -o imgsrch.tar.gz \
  "https://github.com/sanmathigb/nrvna/releases/latest/download/$kit.tar.gz"
tar -xzf imgsrch.tar.gz
cd "$kit"
```

Keep `imgsrch` and `bin/` together. On macOS, if the unsigned developer
preview is blocked, remove its quarantine attribute once from this directory:

```bash
xattr -dr com.apple.quarantine .
```

## 2. Fetch and verify models

Tell the person before starting: this downloads about 3.4 GB once.

```bash
./imgsrch setup
./imgsrch doctor
```

`setup` checksum-verifies three logical models: caption, OCR, and embedding.
Caption and OCR each include a projector, so five GGUF files appear on disk.
If a download is interrupted, run `setup` again. Do not continue until
`doctor` reports the engine and all models ready.

## 3. Index images

Ask which folder contains the images. Keep the searchable project outside the
application directory; use `~/imgsrch-shots` unless they choose another path.
The glob below imports supported files directly inside that folder; it does not
recurse into subdirectories.

```bash
./imgsrch init ~/imgsrch-shots
./imgsrch index ~/imgsrch-shots "/path/to/images"/*
./imgsrch status ~/imgsrch-shots
```

`init` safely reuses an existing project. Expect `index` or `status` to report
at least one image; if it reports zero, ask for the correct source folder
instead of claiming success. Run `status` once only. Indexing continues durably
in the background after the agent and terminal exit. Do not poll, sleep,
inspect processes or logs, or wait for completion.

## 4. Hand off

Use the single status result you already collected. If every image is indexed,
say setup and indexing are complete. Otherwise say indexing has started and
continues locally. In either case, the person may close the terminal. Give
these commands for later, but do not run them unless asked:

```bash
./imgsrch status ~/imgsrch-shots
./imgsrch search ~/imgsrch-shots "that diagram about how a KV cache works"
```

When they return, check status once. If nothing is indexed yet, report that
indexing is still running and stop. Otherwise search. Results name their
source files, and a preview report is written to
`~/imgsrch-shots/search-results.md`.
