# Rendered with:
#   nrvna 0.1.1
#   VHS v0.11.0 (c6af91a)
#   ttyd 1.7.7
#   ffmpeg 8.1.2
#   macOS 13.7.5, x86_64
#
# Render from the repository root:
#   PATH="/path/to/nrvna/bin:/path/to/vhs-tools:$PATH" \
#   vhs demos/submit-without-daemon.tape

Require wrk
Require nrvnad
Require flw
Require ffmpeg
Require ttyd

Output assets/submit-without-daemon.gif

Source demos/vhs-style.tape

Env LANG "C"
Env LC_ALL "C"

Hide
Type `export PS1='\[\e[38;2;240;138;75m\]$\[\e[0m\] '; unset PROMPT_COMMAND` Enter
Wait+Line /\$$/
Type `work=$(mktemp -d /tmp/nrvna-vhs.XXXXXX) || exit 1; cd "$work" || exit 1` Enter
Wait+Line /\$$/
Type `printf '\033c'` Enter
Wait+Line /\$$/
Show

Type `nrvnad status ./workspace` Enter
Wait+Line /\$$/
Type `wrk ./workspace "Say only: hello"` Enter
Wait+Line /\$$/
Type `flw ./workspace --json` Enter
Wait+Line /\$$/
Sleep 2s

Hide
Type `cd /; rm -rf "$work"` Enter
