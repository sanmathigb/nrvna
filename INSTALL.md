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

The installer does not download a model. It installs matching guides under
`$HOME/.local/share/nrvna` and prints their location.

For an isolated installation, download the script and choose a directory:

```sh
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh -o install.sh
NRVNA_INSTALL_DIR="$PWD/bin" sh install.sh
```

Guides then go under `./share/nrvna`. Read the installed `AGENTS.md` before
handing work to an agent.

Check `nrvnad --version` and `nrvnad --help`. Keep all three commands and their
guides from the same release. Use the installed help when command syntax differs.

For a downloaded release archive and checksum in `./releases`, install locally:

```sh
NRVNA_RELEASE_BASE_URL="file://$PWD/releases" NRVNA_INSTALL_DIR="$PWD/bin" sh install.sh
```

Use the installer and guides from that release. This path also supports testing
an archive from a local directory.

## Verify the commands

You can verify the commands without a model. Submission does not need one.

```sh
wrk ./check "Test prompt."
flw ./check
nrvnad status ./check
```

`wrk` prints a job ID and returns at once. `flw` shows the job queued, and
`nrvnad status` exits `1` because no daemon runs yet. This confirms the three
commands work. Processing the job needs a model, which you choose next.

## Choose a model for your hardware

Add your hardware to your Hugging Face profile. Model pages can then estimate
which GGUF quantizations fit that hardware. The profile is public by default;
you can make it private in the hardware settings.

- [Add hardware](https://huggingface.co/settings/hardware)
- [Browse GGUF models for llama.cpp](https://huggingface.co/models?library=gguf&apps=llama.cpp,llama-app&sort=trending)

Treat the compatibility panel as a memory estimate, not a speed guarantee.
Read the model card and license before you download a model. For vision models,
also find the matching projector file in the same model repository.

nrvna does not choose or download models. Download the GGUF files, then pass
their local paths to `nrvnad`. llama.cpp estimates GPU placement from available
memory when the build supports a GPU backend. The estimate is not a speed test.
Set `NRVNA_GPU_LAYERS=0` to force CPU placement. This release runs TTS models
and vocoders on the CPU.

## Use model-specific settings

Before you use another model, read its Hugging Face model card. Also read
`tokenizer_config.json` and `generation_config.json` when the repository has
them. Choose a GGUF that matches the model and quantization you want.

llama.cpp reads the chat template stored in GGUF metadata. nrvna uses that
template by default. Use `NRVNA_CHAT_TEMPLATE_FILE` only when the model needs a
separate Jinja template file.
TranslateGemma's `source to target: text` format uses the repository's
`models/templates/translategemma.jinja`. The 0.2.1 binary archive does not include
this file. Download the template from the matching release:

```sh
curl -fsSL https://raw.githubusercontent.com/sanmathigb/nrvna/v0.2.1/models/templates/translategemma.jinja -o translategemma.jinja
```

Set `NRVNA_CHAT_TEMPLATE_FILE` to the downloaded file's absolute path before
starting `nrvnad`. A source checkout can use its existing template.

nrvna uses sampling values from GGUF metadata when they exist. It does not
automatically read Hugging Face JSON files or model-card prose. Set supported
`NRVNA_*` values from the model's recommendations when they differ. See
[CONFIGURATION.md](CONFIGURATION.md) for supported settings and precedence.

Before a larger run:

1. Record the model revision, quantization, projector, template, and explicit settings.
2. Start from the model author's recommendations. Use only settings listed in [CONFIGURATION.md](CONFIGURATION.md).
3. Try a small representative batch and compare every required fact with its source.
4. Check output truncation, missing facts, and invented claims separately from job status and JSON validity.
5. Change one setting or prompt choice at a time. Check the change on fresh inputs.

For example, a model card may show temperature `0.2` and top-p `0.9` for sampled
generation. Such values are specific to that model and do not guarantee
extraction quality. Set supported overrides on the daemon command, not on `wrk`:

```sh
NRVNA_TEMP=0.2 NRVNA_TOP_P=0.9 nrvnad ./workspace ./models/your-model.gguf --workers 1 --drain
```

Check daemon status first. Settings take effect when that daemon starts; they
do not change an already running daemon. Use installed help for argument order.
Greedy decoding (`NRVNA_TEMP=0`) can make comparisons repeatable, but some models
recommend sampling. Keep their documented constraints.

For small models, split a difficult extraction into bounded questions when
that preserves the needed context. Keep pronoun referents and shared evidence
in each affected prompt. Submit all independent pieces before waiting. Validate the combined coverage. Do not silently replace the model's
answer with your own and count that as a successful run.

Keep the input and output budgets close to the task. Each job gets a context
sized for its prompt and output. `NRVNA_MAX_CTX` is a limit; it does not
reserve the full context for every job.

## Run one job

```bash
wrk ./workspace "Reply with exactly: first"
nrvnad ./workspace ./models/your-model.gguf --drain
flw ./workspace JOB_ID
```

Replace `JOB_ID` with the ID printed by `wrk`. Check that the model replies
`first`; output can vary.
`wrk` creates the workspace when needed.

## Choose a run mode

Use drain mode for a bounded batch. It loads the model, processes queued jobs,
then exits:

```bash
nrvnad ./workspace ./models/your-model.gguf --drain
```

Use a persistent daemon when you expect repeated work. Check the workspace
first. Start one daemon only when status reports that none is running:

```bash
nrvnad status ./workspace
nrvnad ./workspace ./models/your-model.gguf
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

For an existing checkout without its backend, initialize the submodule first:

```sh
git submodule update --init --recursive
```

Then repeat the CMake commands. Source builds put the commands under `./build`:

```bash
./build/wrk ./workspace "Reply with exactly: first"
./build/nrvnad ./workspace ./models/your-model.gguf --drain
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
