# Install nrvna

## Install prebuilt binaries

The installer adds `nrvnad`, `wrk`, and `flw` to `$HOME/.local/bin`.

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
```

Add the directory to `PATH` when the installer asks:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

The installer does not download a model.

## Get an example model

nrvna runs GGUF models supported by its llama.cpp build. Use a compatible
model that you already have, or use this verified example.

The example is SmolLM2 1.7B Q4_K_M. The download is about 1 GB. The model uses
the Apache-2.0 license. It is an example, not a limit.

```bash
mkdir -p models
curl -fL --continue-at - -o models/smollm2-1.7b.gguf \
  https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/2d4a76a30b4af41ecd395c35725ac11688d4cfe4/smollm2-1.7b-instruct-q4_k_m.gguf

MODEL_SHA256=decd2598bc2c8ed08c19adc3c8fdd461ee19ed5708679d1c54ef54a5a30d4f33
if command -v sha256sum >/dev/null; then
  echo "$MODEL_SHA256  models/smollm2-1.7b.gguf" | sha256sum -c -
else
  echo "$MODEL_SHA256  models/smollm2-1.7b.gguf" | shasum -a 256 -c -
fi
```

## Run one job

```bash
job=$(wrk ./workspace "Reply with exactly: first")
nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain
flw ./workspace "$job"
```

The result is `first`. `wrk` creates the workspace when needed.

## Build from source

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build
cmake --build build -j4
```

Source builds put the commands under `./build`:

```bash
job=$(./build/wrk ./workspace "Reply with exactly: first")
./build/nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain
./build/flw ./workspace "$job"
```

## Inspect an archive before installation

| Platform | Archive |
| --- | --- |
| Apple Silicon | `nrvna-darwin-arm64.tar.gz` |
| Intel Mac | `nrvna-darwin-x86_64.tar.gz` |
| x86-64 Linux | `nrvna-linux-x86_64.tar.gz` |

```bash
mkdir -p ~/nrvna-app
cd ~/nrvna-app

case "$(uname -sm)" in
  "Darwin arm64")  kit=nrvna-darwin-arm64 ;;
  "Darwin x86_64") kit=nrvna-darwin-x86_64 ;;
  "Linux x86_64")  kit=nrvna-linux-x86_64 ;;
  *)
    echo "nrvna: unsupported platform: $(uname -sm)" >&2
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
  echo "nrvna: archive checksum failed" >&2
  exit 1
fi

tar -xzf "$kit.tar.gz"
cd "$kit"
```

Run the packaged commands from `./bin`, or move them to a directory on `PATH`.

If macOS blocks the first launch, approve the binaries in **System Settings >
Privacy & Security**.
