#!/bin/sh

set -eu

repo=${NRVNA_REPO:-sanmathigb/nrvna}
base_url=${NRVNA_RELEASE_BASE_URL:-https://github.com/$repo/releases/latest/download}
install_dir=${NRVNA_INSTALL_DIR:-"$HOME/.local/bin"}
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-install.XXXXXX")

cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT HUP INT TERM

case "$(uname -sm)" in
  "Darwin arm64")  kit=nrvna-darwin-arm64 ;;
  "Darwin x86_64") kit=nrvna-darwin-x86_64 ;;
  "Linux x86_64")  kit=nrvna-linux-x86_64 ;;
  *)
    echo "nrvna: unsupported platform: $(uname -sm)" >&2
    exit 1
    ;;
esac

archive="$tmpdir/$kit.tar.gz"
checksum="$tmpdir/$kit.tar.gz.sha256"

curl -fL -o "$archive" "$base_url/$kit.tar.gz"
curl -fL -o "$checksum" "$base_url/$kit.tar.gz.sha256"

expected=$(awk 'NR == 1 {print $1}' "$checksum")
if command -v shasum >/dev/null 2>&1; then
  actual=$(shasum -a 256 "$archive" | awk '{print $1}')
else
  actual=$(sha256sum "$archive" | awk '{print $1}')
fi

if [ "$actual" != "$expected" ]; then
  echo "nrvna: archive checksum failed" >&2
  exit 1
fi

mkdir -p "$install_dir"
tar -C "$tmpdir" -xzf "$archive"
for bin in nrvnad wrk flw; do
  install -m 755 "$tmpdir/$kit/bin/$bin" "$install_dir/$bin"
done

case ":$PATH:" in
  *":$install_dir:"*) ;;
  *)
    echo "nrvna: $install_dir is not on your PATH." >&2
    echo "Add it with:" >&2
    echo "  export PATH=\"$install_dir:\$PATH\"" >&2
    ;;
esac

cat <<EOF
nrvna: installed nrvnad, wrk, and flw to $install_dir

Next:
  export PATH="$install_dir:\$PATH"
  mkdir -p models
  curl -fL --continue-at - -o models/smollm2-1.7b.gguf \\
    https://huggingface.co/HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF/resolve/2d4a76a30b4af41ecd395c35725ac11688d4cfe4/smollm2-1.7b-instruct-q4_k_m.gguf
  MODEL=models/smollm2-1.7b.gguf
  WS=\$(mktemp -d "\${TMPDIR:-/tmp}/nrvna-demo.XXXXXX")
  JOB=\$(wrk "\$WS" "Reply with exactly: first")
  nrvnad "\$MODEL" "\$WS" --drain
  flw "\$WS" "\$JOB"
EOF
