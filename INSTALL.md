# Installing nrvna

Use the one-line installer if you want the binaries on `PATH`.

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
```

The script installs `nrvnad`, `wrk`, and `flw`. It does not download a model.
You still need one local GGUF path before you can run inference. If you do not
have one, use the example model in [QUICKSTART.md](QUICKSTART.md).

Use the manual path if you want to inspect the release archive first.

## Supported archives

| Platform | Archive |
| --- | --- |
| Apple Silicon | `nrvna-darwin-arm64.tar.gz` |
| Intel Mac | `nrvna-darwin-x86_64.tar.gz` |
| x86-64 Linux | `nrvna-linux-x86_64.tar.gz` |

## Download and verify

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

## First useful run

Pick a local GGUF model and run one job:

```bash
MODEL=/path/to/model.gguf
WS=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-demo.XXXXXX")

JOB=$(./bin/wrk "$WS" "Reply with exactly: first")
./bin/nrvnad "$MODEL" "$WS" --drain
./bin/flw "$WS" "$JOB"
```

If macOS blocks the first launch, approve the binaries in **System Settings →
Privacy & Security**.
