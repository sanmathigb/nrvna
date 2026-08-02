# Setting up imgsrch (for an AI agent)

You are an AI coding agent. Set up `imgsrch` for the person you help. Run each
step and verify its result. Ask a question only when this guide tells you.
Indexing and search run locally. Installation downloads release and model files.
Do not ask for an account, API key, or secret.

## 1. Install

Run `uname -sm` to identify the platform. Download the matching archive from
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

curl -fL -o "$kit.tar.gz" \
  "https://github.com/sanmathigb/nrvna/releases/latest/download/$kit.tar.gz"
curl -fL -o "$kit.tar.gz.sha256" \
  "https://github.com/sanmathigb/nrvna/releases/latest/download/$kit.tar.gz.sha256"

expected=$(awk 'NR == 1 {print $1}' "$kit.tar.gz.sha256")
if command -v shasum >/dev/null 2>&1; then
  actual=$(shasum -a 256 "$kit.tar.gz" | awk '{print $1}')
else
  actual=$(sha256sum "$kit.tar.gz" | awk '{print $1}')
fi
if [ "$actual" != "$expected" ]; then
  echo "imgsrch: archive checksum failed" >&2
  exit 1
fi

tar -xzf "$kit.tar.gz"
cd "$kit"
```

Keep `imgsrch` and `bin/` together. macOS can block the unsigned preview. If it
does, stop and ask the person to approve it in **System Settings → Privacy &
Security**. Do not remove quarantine metadata.

## 2. Fetch and verify models

Tell the person before starting: this downloads about 3.4 GB once.

```bash
./imgsrch setup
./imgsrch doctor
```

`setup` verifies three models: caption, OCR, and embedding. Caption and OCR
each use a projector. The setup therefore creates five GGUF files. Run `setup`
again after an interrupted download. Continue only when `doctor` reports that
the engine and all models are ready.

## 3. Index images

Ask which folder contains the images. Keep the project outside the application
directory. Use `~/imgsrch-shots` unless the person chooses another path. The
glob imports files directly from the source folder. It does not search
subdirectories.

```bash
./imgsrch init ~/imgsrch-shots
./imgsrch index ~/imgsrch-shots "/path/to/images"/*
./imgsrch status ~/imgsrch-shots
```

`init` safely reuses an existing project. `index` or `status` must report at
least one image. If it reports zero, ask for the correct source folder. Do not
claim success. Run `status` once. Indexing continues after the agent and
terminal exit. Do not poll, sleep, inspect processes, inspect logs, or wait.

## 4. Hand off

Use the status result that you collected. If every image is indexed, report
completion. Otherwise, report that local indexing continues. The person can
close the terminal in both cases. Give these commands for later. Do not run
them unless the person asks.

```bash
./imgsrch status ~/imgsrch-shots
./imgsrch search ~/imgsrch-shots "that diagram about how a KV cache works"
```

When the person returns, check status once. If nothing is indexed, report that
indexing continues and stop. Otherwise, search. Results name their source
files. imgsrch writes previews to `~/imgsrch-shots/search-results.md`.
