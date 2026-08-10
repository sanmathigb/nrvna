# Rendered with:
#   nrvna 0.1.1
#   VHS v0.11.0 (c6af91a)
#   ttyd 1.7.7
#   ffmpeg 7.1.1
#   macOS 13.7.5, x86_64
#   MacBookPro14,3, Intel Core i7 3.1 GHz, 16 GB
#
# The model is SmolLM2 1.7B Q4_K_M at the revision and checksum in README.md.
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

Set Shell "bash"
Set Width 960
Set Height 540
Set FontSize 20
Set Padding 28
Set Framerate 12
Set TypingSpeed 25ms
Set CursorBlink false
Set Theme { "name": "nrvna", "black": "#16130F", "red": "#E06C75", "green": "#98C379", "yellow": "#E5C07B", "blue": "#61AFEF", "magenta": "#C678DD", "cyan": "#56B6C2", "white": "#D8D3C8", "brightBlack": "#6E6A62", "brightRed": "#E06C75", "brightGreen": "#98C379", "brightYellow": "#E5C07B", "brightBlue": "#61AFEF", "brightMagenta": "#C678DD", "brightCyan": "#56B6C2", "brightWhite": "#FCFBF9", "background": "#16130F", "foreground": "#D8D3C8", "selection": "#3A332C", "cursor": "#F08A4B" }

Env LANG "C"
Env LC_ALL "C"
Env NRVNA_PREDICT "32"
Env NRVNA_THINKING "0"

Hide
Type `export PS1='\[\e[38;2;240;138;75m\]$\[\e[0m\] '; unset PROMPT_COMMAND` Enter
Wait+Line /\$$/
Type `test -f "$NRVNA_DEMO_MODEL" || exit 1` Enter
Wait+Line /\$$/
Type `work=$(mktemp -d /tmp/nrvna-vhs.XXXXXX); cd "$work"` Enter
Wait+Line /\$$/
Type `mkdir models; ln -s "$NRVNA_DEMO_MODEL" models/smollm2-1.7b.gguf` Enter
Wait+Line /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `model=./models/smollm2-1.7b.gguf` Enter
Wait+Line /\$$/
Type `first=$(wrk ./demo "Write numbered lines until output stops")` Enter
Wait+Line /\$$/
Type `second=$(wrk ./demo "Output only: second. Do not add quotes or punctuation.")` Enter
Wait+Line /\$$/
Type `third=$(wrk ./demo "Output only: third. Do not add quotes or punctuation.")` Enter
Wait+Line /\$$/
Type `flw ./demo --json` Enter
Wait+Line /\$$/
Sleep 600ms

Type `nrvnad "$model" ./demo --workers 1 >daemon.log 2>&1 & pid=$!` Enter
Wait+Line /\$$/
Hide
Type `disown "$pid"; for i in {1..600}; do [[ -d "./demo/processing/$first" ]] && break; sleep 0.1; done; [[ -d "./demo/processing/$first" ]] || exit 1; printf '\033c'` Enter
Wait+Line@60s /\$$/
Show

Type `flw ./demo --json` Enter
Wait+Line /\$$/
Sleep 300ms
Type `[[ -d "./demo/processing/$first" ]] && kill -9 "$pid"` Enter
Wait+Line /\$$/
Sleep 400ms
Type `nrvnad status ./demo` Enter
Wait+Line /\$$/
Sleep 300ms
Type `ls ./demo/processing/` Enter
Wait+Line /\$$/
Sleep 700ms

Type `nrvnad "$model" ./demo --workers 1 --drain >restart.log 2>&1`
Sleep 500ms
Hide
Enter
Wait+Line@120s /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `flw ./demo --json` Enter
Wait+Line /\$$/
Type `flw ./demo "$first" >/dev/null && echo "first: recovered"` Enter
Wait+Line /\$$/
Type `flw ./demo "$second"` Enter
Wait+Line /\$$/
Type `flw ./demo "$third"` Enter
Wait+Line /\$$/
Sleep 2s

Hide
Type `cd /; rm -rf "$work"` Enter
