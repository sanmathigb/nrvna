---
name: nrvna
description: Run durable local AI work through nrvna's CLI primitives. Use when the user invokes /nrvna with a prompt, model, and workspace, or when local inference fits private, multimodal, batch, or non-urgent work.
compatibility: Requires the nrvna CLI, shell access, and a compatible local GGUF model.
metadata:
  opencode/slash: "true"
---

# nrvna

Use nrvna to run local inference as durable jobs. Jobs and results stay in a
workspace. A bounded drain or persistent daemon runs the model. Use the CLI
directly for precise control; use this skill to invoke it from the conversation.

Use the model and workspace the user names. Do not choose, download, or install a model.

## Invoke

Use the invocation form supported by the agent host:

- Claude Code: `/nrvna`
- Codex: `$nrvna`
- Pi: `/skill:nrvna`
- OpenCode: `/nrvna` or ask the agent to use the `nrvna` skill

Other Agent Skills hosts can load this folder when they support the standard
`SKILL.md` format and local shell execution.

Pass the prompt, model, and workspace in either form:

```text
<skill> <prompt>
<skill> --model <GGUF path or known model name> --workspace <path> <prompt>
```

Example:

```text
$nrvna --model ~/models/qwen-coder.gguf --workspace ~/nrvna/code-review "Find every unchecked error path in these files."
```

Ask for any missing model or workspace.
Reuse values the user clearly established earlier.
Never infer a model from the task.

Give a GGUF path or a model name known to nrvna.
Names resolve under `./models` or `NRVNA_MODELS_DIR`.
The workspace is a directory path.
These are skill arguments, not `wrk` options.

## Submit one prompt

1. Set `BIN`: use `dirname "$(command -v wrk)"` when available, else `./build` in source or `./bin` in a package.
   Stop if any command is missing. Tell the user how to install nrvna; do not install it.
2. Check `"$BIN/nrvnad" status "$WORKSPACE" --json` before submission.
   Read the exit code: `0` ready, `2` starting, `1` not running.
3. A starting daemon does not report its model. Return without submitting
   or waiting. Ask the user to invoke the skill again when it is ready.
4. If ready, confirm the daemon uses the requested model. Do not submit on mismatch; never stop a user-started daemon.
5. Submit the prompt and save the job ID:

   ```bash
   JOB=$("$BIN/wrk" "$WORKSPACE" "$PROMPT" --tag "$TAG")
   ```

   Set these shell values from the user's inputs. Quote each value.
   Use a short unique tag for this invocation.
   Keep diagnostics off the job-ID output.
6. If the daemon is ready, use one of these completion modes:

   - Push: to report completion, start this waiter as a host-managed task.
     Its exit triggers the host event:

     ```bash
     "$BIN/flw" "$WORKSPACE" "$JOB" -w
     ```

     For a batch, use `"$BIN/flw" "$WORKSPACE" -W --tag "$TAG"`.
     Track `flw` itself. Do not add `nohup` or `&`.

   - Pull: start no waiter. Return the IDs; the user can request `collect` later.
7. If no daemon runs, start `nrvnad "$WORKSPACE" "$MODEL" --drain` as a
   host-managed task. Its exit signals an idle queue. Do not add `nohup` or `&`.
   Use a detached shell only if known to persist; it gives no completion event.
   Otherwise leave the job queued.
8. Return workspace, tag, job ID, and task handle. Collect on event; otherwise
   tell the user to request `collect` later. Never poll.

Never run a drain or job wait in the foreground.
Do not stop a daemon the user started.

## Collect

For `collect <workspace> <tag>`, read job states and results once:

```bash
"$BIN/flw" "$WORKSPACE" --tag "$TAG" --json
```

Report completed, failed, queued, and running jobs. Do not poll; collect on a
host completion event or user request.
Check source references before calling findings verified.

## Rules

- Each job has fresh context. Put all needed instructions and evidence in its prompt.
- Use one workspace per model role. Do not change a workspace's model while its daemon is running.
- Jobs and results persist as files. Inference progresses only while a daemon runs on an awake machine.
- A crash can restart a job with fresh context. Generated tokens do not resume.
- `--parent` records lineage only. It does not pass context or set execution order.
- nrvna does not send completion events. The host reports a finite task's exit.
- Use [`acceptance-rubric.md`](acceptance-rubric.md) and [`findings.schema.json`](findings.schema.json) when the task needs structured per-file findings.
