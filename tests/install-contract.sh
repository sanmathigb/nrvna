#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: install-contract.sh <binary-dir>}"
source_dir="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
case "$(uname -sm)" in
  'Darwin arm64') kit=nrvna-darwin-arm64 ;;
  'Darwin x86_64') kit=nrvna-darwin-x86_64 ;;
  'Linux x86_64') kit=nrvna-linux-x86_64 ;;
  *) echo 'Unsupported installer test platform' >&2; exit 1 ;;
esac
mkdir -p "$tmp/release/$kit/bin" "$tmp/release/$kit/docs" "$tmp/release/$kit/skills"
cp "$bin_dir/wrk" "$bin_dir/nrvnad" "$bin_dir/flw" "$tmp/release/$kit/bin/"
cp "$source_dir/AGENTS.md" "$source_dir/INSTALL.md" "$tmp/release/$kit/"
cp "$source_dir/docs/how-it-works.md" "$tmp/release/$kit/docs/"
cp -R "$source_dir/skills/nrvna" "$tmp/release/$kit/skills/"
tar -C "$tmp/release" -czf "$tmp/release/$kit.tar.gz" "$kit"
(cd "$tmp/release" && shasum -a 256 "$kit.tar.gz") > "$tmp/release/$kit.tar.gz.sha256"
NRVNA_INSTALL_DIR="$tmp/install/bin" NRVNA_RELEASE_BASE_URL="file://$tmp/release" \
  sh "$source_dir/install.sh" > "$tmp/install.log" 2>&1
for command in wrk nrvnad flw; do
  cmp "$bin_dir/$command" "$tmp/install/bin/$command"
  "$tmp/install/bin/$command" --version
done
cmp "$source_dir/AGENTS.md" "$tmp/install/share/nrvna/AGENTS.md"
test -f "$tmp/install/share/nrvna/docs/how-it-works.md"
test -f "$tmp/install/share/nrvna/skills/nrvna/SKILL.md"
grep -q 'AGENTS.md' "$tmp/install.log"

# A failed verification must preserve a previous installation.
printf 'bad-checksum  %s.tar.gz\n' "$kit" > "$tmp/release/$kit.tar.gz.sha256"
if NRVNA_INSTALL_DIR="$tmp/install/bin" NRVNA_RELEASE_BASE_URL="file://$tmp/release" \
  sh "$source_dir/install.sh" > "$tmp/rejected.log" 2>&1; then
  echo 'Installer accepted a mismatched checksum' >&2; exit 1
fi
grep -q 'archive checksum failed' "$tmp/rejected.log"
cmp "$bin_dir/wrk" "$tmp/install/bin/wrk"
echo 'install-contract: all checks passed'
