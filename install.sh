#!/bin/sh

set -eu

repo=${NRVNA_REPO:-sanmathigb/nrvna}
base_url=${NRVNA_RELEASE_BASE_URL:-https://github.com/$repo/releases/latest/download}
install_dir=${NRVNA_INSTALL_DIR:-"$HOME/.local/bin"}
doc_dir="$install_dir/../share/nrvna"
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

mkdir -p "$doc_dir"
for doc in README.md INSTALL.md AGENTS.md CONTEXT.md ADVANCED.md CONFIGURATION.md ARCHITECTURE.md CHANGELOG.md SECURITY.md CONTRIBUTING.md LICENSE LICENSE-llama.cpp; do
  if [ -f "$tmpdir/$kit/$doc" ]; then
    install -m 644 "$tmpdir/$kit/$doc" "$doc_dir/$doc"
  fi
done
for directory in docs skills; do
  if [ -d "$tmpdir/$kit/$directory" ]; then
    cp -R "$tmpdir/$kit/$directory" "$doc_dir/"
  fi
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
Guides: $doc_dir

Next:
  Read $doc_dir/INSTALL.md for your first run.
  Give $doc_dir/AGENTS.md to your agent.
  Use an existing compatible model, or follow the model download guide.
  Check $install_dir/nrvnad --help for this release's syntax.
EOF
