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
Type `set +m; mkfifo daemon.pid daemon.events; (read -r target <daemon.pid; daemon_ready=0; job_running=0; while IFS= read -r line; do [[ $line == *"RUNNING"* ]] && daemon_ready=1; [[ $line == *"  running"* ]] && job_running=1; (( daemon_ready && job_running )) && { kill -STOP "$target"; exit; }; done <daemon.events; exit 1) & watcher=$!` Enter
Wait+Line /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `job=$(wrk ./demo "Say only: hello")` Enter
Wait+Line /\$$/
Sleep 600ms

Type `nrvnad ./models/smollm2-1.7b.gguf ./demo 2>daemon.events & pid=$!` Enter
Wait+Line /\$$/
Hide
Type `printf '%s\n' "$pid" >daemon.pid; disown "$pid"; wait "$watcher"; printf '\033c'` Enter
Wait+Line@60s /\$$/
Show

Type `nrvnad status ./demo` Enter
Wait+Line /\$$/
Type `flw ./demo --json` Enter
Wait+Line /\$$/
Sleep 300ms
Type `kill -9 "$pid"` Enter
Wait+Line /\$$/
Sleep 400ms
Type `nrvnad status ./demo` Enter
Wait+Line /\$$/
Sleep 300ms
Type `ls ./demo/processing/` Enter
Wait+Line /\$$/
Sleep 700ms

Type `nrvnad ./models/smollm2-1.7b.gguf ./demo --drain 2>/dev/null` Enter
Wait+Line@120s /\$$/
Type `flw ./demo "$job"` Enter
Wait+Line /\$$/
Sleep 2s

Hide
Type `cd /; rm -rf "$work"` Enter
