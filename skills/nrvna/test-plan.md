# nrvna skill test plan

Test skill behavior separately from nrvna’s value. A successful handoff does
not prove that local work is useful or economical.

## 1. Skill behavior

Use the same agent, model, task, and repository snapshot in separate sessions.
Run once with the skill available and once without it. Randomize the order.

Include one task suited to a local batch and one task that needs live reasoning.
Score correct routing, CLI use, result verification, and unnecessary delegation.
Record paid-model usage, elapsed time, failures, and human corrections.

For `/nrvna`, test a complete invocation with a user-selected model and
workspace, plus an invocation missing one or both values. The skill must ask
for missing choices and must not guess or download a model.

Verify that submission returns without waiting for inference. When a daemon is
starting, verify that the skill returns without submitting or waiting. Verify
that a ready daemon supports both modes: a host-managed background `flw -w`
waiter (or `flw -W` for a tag) that triggers one collection, and pull mode with
no waiter. When no daemon runs, verify the host manages `nrvnad --drain` itself,
or uses a detached shell only when known to persist. Otherwise, verify the job
stays queued. Confirm the drain's completion event triggers one collection.
Without an event, verify the skill tells the user to request `collect` later.
`collect` returns pending states at once.

Start with one agent host. Test other hosts only after this flow works.

Observed on Claude Code (2026-10-18): a persistent `smollm2` daemon processed
a job while a host-managed `flw -w` waiter ran in the background. The waiter
exited successfully at job completion and triggered one-shot collection. The
daemon remained running until stopped. This validates the push path on Claude
Code only.

## 2. Workflow value

Use a useful, repeatable task with a pinned source snapshot, explicit inputs,
and expected findings prepared without showing them to the models. The existing
three-file recovery case is a smoke test, not a value benchmark.

Compare normal agent work, qmd retrieval, frontier subagents, discounted cloud
batch, and nrvna jobs when each can run the same task. Use the same acceptance
rubric. Do not treat an unsuitable arm’s failure as evidence for nrvna.

Report accepted findings, coverage, failures, human effort, and elapsed time.
Record provider-reported input, cached-input, and output usage. Report local
runtime and measured power separately from paid-model cost. Show hardware cost
assumptions separately; do not present one assumed amortization as universal.

## Decision

Improve the skill if agents misroute or misuse the workflow. Keep a workflow
only if it produces accepted results and offers a worthwhile cost, privacy, or
operational benefit. One task is a screening result, not general proof.
