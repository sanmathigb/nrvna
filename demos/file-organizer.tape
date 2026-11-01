# Rendered with:
#   nrvna 0.1.1
#   VHS v0.11.0
#   ttyd 1.7.7
#   ffmpeg 9.0.1
#   macOS 13.7.5, x86_64
#   MacBookPro14,3, Intel Core i7 3.1 GHz, 16 GB
#
# Model:
#   LiquidAI/LFM2.5-2.6B-GGUF
#   revision b22e29ebf6249a8c9fcdda36914743e9980595c4
#   LFM2.5-2.6B-Q4_K_M.gguf
#   SHA-256 79fdf00351b46cf26f020aead28d01889886be87c55fa0eb907e6f9b00bfee14
#
# The tape hides only the model wait.
# Render from the repository root:
#   PATH="$PWD/build:/path/to/vhs-tools:$PATH" \
#   NRVNA_DEMO_MODEL="/path/to/LFM2.5-2.6B-Q4_K_M.gguf" \
#   vhs demos/file-organizer.tape

Require wrk
Require nrvnad
Require flw
Require file-organizer-validator
Require ffmpeg
Require ttyd

Output site/assets/file-organizer.gif

Source demos/vhs-style.tape

Set FontSize 18
Set TypingSpeed 35ms

Env LANG "C"
Env LC_ALL "C"
Env NRVNA_TEMP "0.1"
Env NRVNA_TOP_K "50"
Env NRVNA_REPEAT_PENALTY "1.1"
Env NRVNA_THINKING "0"

Hide
Type `export PS1='\[\e[38;2;240;138;75m\]$\[\e[0m\] '; unset PROMPT_COMMAND` Enter
Wait+Line /\$$/
Type `test -f "$NRVNA_DEMO_MODEL" || exit 1` Enter
Wait+Line /\$$/
Type `work=$(mktemp -d /tmp/nrvna-vhs.XXXXXX) || exit 1; cp -R examples/file-organizer "$work/" || exit 1; cd "$work/file-organizer" || exit 1` Enter
Wait+Line /\$$/
Type `alias nrvnad='nrvnad 2>/dev/null'` Enter
Wait+Line /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `find inbox -type f | sort` Enter
Wait+Line /\$$/
Sleep 700ms

Type `job=$(wrk ./workspace - --json-schema move-plan.schema.json < request.txt)` Enter
Wait+Line /\$$/
Type `echo "$job"` Enter
Wait+Line /\$$/
Sleep 500ms

Type `nrvnad ./workspace "$NRVNA_DEMO_MODEL" --drain`
Hide
Enter
Wait+Line@120s /\$$/
Show

Type `flw ./workspace "$job" > plan.json` Enter
Wait+Line /\$$/
Type `cat plan.json` Enter
Wait+Line /\$$/
Sleep 1s

Type `file-organizer-validator ./inbox plan.json Release Encoders` Enter
Wait+Line /\$$/
Sleep 700ms
Type `file-organizer-validator --apply ./inbox plan.json Release Encoders` Enter
Wait+Line /\$$/
Sleep 700ms

Type `find inbox -type f | sort` Enter
Wait+Line /\$$/
Sleep 2s

Hide
Type `cd /; rm -rf "$work"` Enter
