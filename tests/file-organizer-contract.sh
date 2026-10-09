#!/usr/bin/env bash
set -euo pipefail

validator=$1
example=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/nrvna-file-organizer.XXXXXX")
trap 'rm -rf "$work"' EXIT

copy_inbox() {
    cp -R "$example/inbox" "$1"
}

snapshot() {
    (cd "$1" && { find . -type d -print; find . -type f -exec shasum -a 256 {} \;; } | sort)
}

expect_unchanged_failure() {
    local inbox=$1
    local plan=$2
    local before after
    before=$(snapshot "$inbox")
    if "$validator" --apply "$inbox" "$plan" Release Encoders >/dev/null 2>&1; then
        echo "expected validator failure: $plan" >&2
        exit 1
    fi
    after=$(snapshot "$inbox")
    test "$before" = "$after"
}

copy_inbox "$work/dry"
before=$(snapshot "$work/dry")
"$validator" "$work/dry" "$example/expected-plan.json" Release Encoders >/dev/null
test "$before" = "$(snapshot "$work/dry")"

copy_inbox "$work/apply"
"$validator" --apply "$work/apply" "$example/expected-plan.json" Release Encoders >/dev/null
test -f "$work/apply/Release/v0.1.1-release-notes.md"
test -f "$work/apply/Release/release-checklist.txt"
test -f "$work/apply/Encoders/h264-encoder-notes.md"
test -f "$work/apply/Encoders/hevc-rate-control.md"
test -f "$work/apply/team-lunch.txt"
test -f "$work/apply/scratchpad.md"
"$validator" --apply "$work/apply" "$example/expected-plan.json" Release Encoders >/dev/null

copy_inbox "$work/partial"
mkdir "$work/partial/Release"
mv "$work/partial/v0.1.1-release-notes.md" "$work/partial/Release/"
"$validator" --apply "$work/partial" "$example/expected-plan.json" Release Encoders >/dev/null
test -f "$work/partial/Encoders/hevc-rate-control.md"

printf '%s\n' '{"moves":[],"review":["team-lunch.txt","scratchpad.md"]}' > "$work/omitted.json"
copy_inbox "$work/invalid-omitted"
expect_unchanged_failure "$work/invalid-omitted" "$work/omitted.json"

printf '%s\n' '{"moves":[{"file":"team-lunch.txt","category":"Release"}],"review":["team-lunch.txt","v0.1.1-release-notes.md","release-checklist.txt","h264-encoder-notes.md","hevc-rate-control.md","scratchpad.md"]}' > "$work/duplicate.json"
copy_inbox "$work/invalid-duplicate"
expect_unchanged_failure "$work/invalid-duplicate" "$work/duplicate.json"

printf '%s\n' '{"moves":[{"file":"../outside","category":"Release"}],"review":["v0.1.1-release-notes.md","release-checklist.txt","h264-encoder-notes.md","hevc-rate-control.md","team-lunch.txt","scratchpad.md"]}' > "$work/traversal.json"
copy_inbox "$work/invalid-traversal"
expect_unchanged_failure "$work/invalid-traversal" "$work/traversal.json"

copy_inbox "$work/invalid-collision"
mkdir "$work/invalid-collision/Release"
cp "$work/invalid-collision/v0.1.1-release-notes.md" "$work/invalid-collision/Release/"
expect_unchanged_failure "$work/invalid-collision" "$example/expected-plan.json"

copy_inbox "$work/invalid-symlink"
ln -s scratchpad.md "$work/invalid-symlink/link.md"
expect_unchanged_failure "$work/invalid-symlink" "$example/expected-plan.json"
