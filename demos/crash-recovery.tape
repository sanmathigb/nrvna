# Rendered with:
#   nrvna 0.1.1
#   VHS v0.11.0 (c6af91a)
#   ttyd 1.7.7
#   ffmpeg 8.1.2
#   macOS 13.7.5, x86_64
#   MacBookPro14,3, Intel Core i7 3.1 GHz, 16 GB
#
# The model is SmolLM2 1.7B Q4_K_M at the revision and checksum in README.md.
# The tape pauses the daemon after it becomes ready and starts the job. This
# makes the render deterministic while the visible commands are typed.
# Render from the repository root:
#   PATH="/path/to/nrvna/bin:/path/to/vhs-tools:$PATH" \
#   NRVNA_DEMO_MODEL="$PWD/models/smollm2-1.7b.gguf" \
#   vhs demos/crash-recovery.tape

Require wrk
Require nrvnad
Require flw
Require ffmpeg
Require ttyd

Output assets/crash-recovery.gif

Source demos/vhs-style.tape

Env LANG "C"
Env LC_ALL "C"
Env NRVNA_PREDICT "8"
Env NRVNA_THINKING "0"

Hide
Type `export PS1='\[\e[38;2;240;138;75m\]$\[\e[0m\] '; unset PROMPT_COMMAND` Enter
Wait+Line /\$$/
Type `test -f "$NRVNA_DEMO_MODEL" || exit 1` Enter
Wait+Line /\$$/
Type `work=$(mktemp -d /tmp/nrvna-vhs.XXXXXX) || exit 1; cd "$work" || exit 1` Enter
Wait+Line /\$$/
Type `mkdir models; ln -s "$NRVNA_DEMO_MODEL" models/smollm2-1.7b.gguf` Enter
Wait+Line /\$$/
Type `set +m; mkfifo daemon.pid; (read -r target <daemon.pid; while :; do active=$(find ./workspace/processing -type d ! -path ./workspace/processing -print -quit 2>/dev/null); if nrvnad status ./workspace >/dev/null 2>&1 && [ -n "$active" ]; then kill -STOP "$target"; exit; fi; sleep 0.02; done) & watcher=$!` Enter
Wait+Line /\$$/
Type `alias nrvnad='nrvnad 2>/dev/null'; sync_demo() { target=$1; printf '%s\n' "$target" >daemon.pid; disown "$target"; wait "$watcher"; printf '\033[2K\033[1A\033[2K\033[1A\033[2K\r'; }` Enter
Wait+Line /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `job=$(wrk ./workspace "Say only: hello")` Enter
Wait+Line /\$$/
Sleep 600ms

Type `nrvnad ./models/smollm2-1.7b.gguf ./workspace &` Enter
Wait+Line /\$$/
Hide
Type `sync_demo "$!"` Enter
Wait+Line@60s /\$$/
Show

Type `nrvnad status ./workspace` Enter
Wait+Line /\$$/
Type `flw ./workspace --json` Enter
Wait+Line /\$$/
Sleep 300ms
Type `kill -9 "$!"` Enter
Wait+Line /\$$/
Sleep 400ms
Type `nrvnad status ./workspace` Enter
Wait+Line /\$$/
Sleep 300ms
Type `ls ./workspace/processing/` Enter
Wait+Line /\$$/
Sleep 700ms

Type `nrvnad ./models/smollm2-1.7b.gguf ./workspace --drain` Enter
Wait+Line@120s /\$$/
Type `flw ./workspace "$job"` Enter
Wait+Line /\$$/
Sleep 2s

Hide
Type `cd /; rm -rf "$work"` Enter
