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

## Get a GGUF model

nrvna runs local GGUF models supported by its llama.cpp build. Use a compatible
GGUF that you already have, or download this verified example. nrvna does not
download models by name or use Hugging Face model IDs directly.

The example is SmolLM2 1.7B Q4_K_M. The download is about 1 GB. The model uses
the Apache-2.0 license. It is an example, not a limit.

```bash
mkdir -p models
curl -fL --continue-at - -o models/smollm2-1.7b.gguf \
  https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/2d4a76a30b4af41ecd395c35725ac11688d4cfe4/smollm2-1.7b-instruct-q4_k_m.gguf

echo "decd2598bc2c8ed08c19adc3c8fdd461ee19ed5708679d1c54ef54a5a30d4f33  models/smollm2-1.7b.gguf" | shasum -a 256 -c -
```

On Linux, replace `shasum -a 256 -c -` with `sha256sum -c -`.

## Use model-specific settings

Before you use another model, read its Hugging Face model card. Also read
`tokenizer_config.json` and `generation_config.json` when the repository has
them. Choose a GGUF that matches the model and quantization you want.

llama.cpp reads the chat template stored in GGUF metadata. nrvna uses that
template by default. Use `NRVNA_CHAT_TEMPLATE_FILE` only when the model needs a
separate Jinja template file.

nrvna uses sampling values from GGUF metadata when they exist. It does not
automatically read Hugging Face JSON files or model-card prose. Set supported
`NRVNA_*` values from the model's recommendations when they differ. See
[CONFIGURATION.md](CONFIGURATION.md) for supported settings and precedence.

Keep the input and output budgets close to the task. Each job gets a context
sized for its prompt and output. `NRVNA_MAX_CTX` is a limit; it does not
reserve the full context for every job.

## Run one job

```bash
wrk ./workspace "Reply with exactly: first"
nrvnad ./workspace ./models/smollm2-1.7b.gguf --drain
flw ./workspace JOB_ID
```

Replace `JOB_ID` with the ID printed by `wrk`. The result is `first`.
`wrk` creates the workspace when needed.

## Choose a run mode

Use drain mode for a bounded batch. It loads the model, processes queued jobs,
then exits:

```bash
nrvnad ./workspace ./models/smollm2-1.7b.gguf --drain
```

Use a persistent daemon when you expect repeated work. Check the workspace
first. Start one daemon only when status reports that none is running:

```bash
nrvnad status ./workspace
nrvnad ./workspace ./models/smollm2-1.7b.gguf
```

Keep the daemon terminal open. In another terminal, submit work and copy the
job ID it prints:

```bash
wrk ./workspace "Summarize this report"
```

Wait for that job and read its result:

```bash
flw ./workspace JOB_ID -w
```

Stop the daemon from another terminal when you finish:

```bash
nrvnad stop ./workspace
```

Use separate workspaces for different model roles. Drain them in sequence when
their models cannot fit in memory together.

## Build from source

```bash
git clone --recursive https://github.com/sanmathigb/nrvna.git
cd nrvna
cmake -S . -B build
cmake --build build -j4
```

Source builds put the commands under `./build`:

```bash
./build/wrk ./workspace "Reply with exactly: first"
./build/nrvnad ./workspace ./models/smollm2-1.7b.gguf --drain
./build/flw ./workspace JOB_ID
```

Replace `JOB_ID` with the ID printed by `wrk`.

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
