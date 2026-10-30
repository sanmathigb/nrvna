# Install nrvna for your coding agents

This guide installs nrvna's local inference commands and agent skill on one
Mac. Repeat the steps on each machine. The skill is portable across Agent
Skills hosts; it does not install the commands or a model.

nrvna provides durable local AI jobs. Use the commands directly, or let a
supported coding agent use them through the skill. Choose a bounded `--drain`
run or a persistent daemon for repeated work. Both keep job records and results
in the workspace.

The agent must run on the Mac that has the model and workspace. A hosted chat
agent cannot read files on your Mac unless its tools run locally.

For a first text job, you need one compatible, instruction-tuned GGUF on that
Mac. You do not need a model to install the skill or engine.

| Step | Downloads | Does not download |
| --- | --- | --- |
| `npx skills add` | Skill folder and installer package | nrvna commands or models |
| nrvna installer | Mac release archive and checksum; installs three commands | GGUF models |
| You provide | Nothing if you already have a compatible GGUF | — |

## 1. Install the skill for your agents

The `skills` installer supports many Agent Skills hosts. This installs `nrvna`
globally for all hosts recognized by the installer:

```bash
npx skills add sanmathigb/nrvna --skill nrvna --global --agent '*' -y
```

This command needs Node.js and npm. [Review the skill files](https://github.com/sanmathigb/nrvna/tree/main/skills/nrvna)
before installing. The skill can run shell commands with your agent's
permissions. The installer links or copies its files into each selected skill
directory. It does not install nrvna or download a model.

If you only want specific agents, omit `--agent '*'` and select them when
prompted. For example, common hosts include Claude Code, Codex, Pi, and
OpenCode. Hosts must support Agent Skills or provide a way to load a `SKILL.md`
and run local shell commands. This does not mean every agent product or hosted
chat environment can run nrvna.

After installation, start a new agent session. Use the host's invocation form:

| Host | Invoke |
| --- | --- |
| Claude Code | `/nrvna` |
| Codex | `$nrvna` |
| Pi | `/skill:nrvna` |
| OpenCode | `/nrvna` or ask the agent to use the `nrvna` skill |

For a manual install, copy the complete `skills/nrvna/` folder into that
host's user skills directory. Common shared locations are `~/.agents/skills/`
for Codex, Pi, and OpenCode, and `~/.claude/skills/` for Claude Code. Keep the
supporting files beside `SKILL.md`. If you have a local nrvna checkout, run:

```bash
mkdir -p "$HOME/.agents/skills" "$HOME/.claude/skills"
cp -R /path/to/nrvna/skills/nrvna "$HOME/.agents/skills/"
cp -R /path/to/nrvna/skills/nrvna "$HOME/.claude/skills/"
```

Check for an existing `nrvna` skill before copying. Do not overwrite a skill
that you installed from another source.

See the [skills installer guide](https://skills.sh/docs/cli) for its current
agent list and options. See the [Agent Skills format](https://agentskills.io/)
for the portable skill format.

## 2. Install the local commands

The commands must be on the same machine as the agent. On supported Macs, use
the installer:

```bash
curl -fsSL https://github.com/sanmathigb/nrvna/raw/main/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

The installer checks the release archive checksum and installs `nrvnad`,
`wrk`, and `flw` in `~/.local/bin`. Add that directory to your shell profile
to keep it on `PATH` in new terminals and agent sessions. The installer
supports Apple Silicon and Intel Macs. See the [main install guide](../../INSTALL.md)
for source builds and Linux packages. The release commands include the
llama.cpp runtime; you do not install llama.cpp separately.

Verify that the agent's shell can find all three commands:

```bash
command -v wrk
command -v nrvnad
command -v flw
```

If the agent cannot find them, restart the agent after updating `PATH`. Do not
assume the interactive terminal and the agent have the same `PATH`.

## 3. Select a model and workspace

nrvna does not include a model. Use a compatible instruction-tuned GGUF you
already have for text generation. Give the skill its full file path. You can also set
`NRVNA_MODELS_DIR` and pass a model name that matches a file there.

The model must match the job type. Vision and speech jobs can need matching
`mmproj` or vocoder files beside the GGUF. `nrvnad --help` lists the job types.

Choose a persistent workspace path for this model role. `wrk` creates the
workspace. Use a different workspace for each model role, and do not change
the model while that workspace's daemon is running.

## 4. Run your first local AI job through the skill

Replace the model path below with a GGUF on your Mac, then invoke the skill in
your agent:

```text
/nrvna --model "/Users/you/Models/model.gguf" --workspace "/Users/you/.local/share/nrvna/workspaces/hello" "Reply with exactly: hello from nrvna"
```

Use `$nrvna` in Codex or `/skill:nrvna` in Pi instead of `/nrvna`. OpenCode
also accepts `/nrvna` when it exposes the skill command.

The skill checks the workspace status, creates the job, and starts a bounded
`--drain` task when no daemon is running. It must not wait in the foreground.
When the host reports that task's exit, the agent collects the result. If the
host does not report background-task completion, ask the agent to collect the
tag later. The expected result is `hello from nrvna`.

## 5. Check or update the installation

In the agent, ask it to run `wrk --version`, `nrvnad --version`, and
`flw --version`, then invoke `$nrvna` or the host's equivalent with a simple
prompt, the GGUF path, and a new workspace. Confirm that it returns a job ID
and collects the saved result.

Update the skill with the same `npx skills add` command when you want the
latest repository version. Update the engine by rerunning the installer. Keep
your GGUF files; neither update downloads or replaces them.

## What is installed

- **Skill:** instructions and supporting files for the agent.
- **Engine:** `nrvnad`, `wrk`, and `flw` for local inference jobs.
- **Model:** supplied by you; one compatible instruction-tuned GGUF is needed
  to run a first text job. Neither installer downloads it.
- **Workspace:** a persistent directory containing queued jobs and results.

Jobs and results persist as files. Inference only progresses while a daemon
runs on an awake machine. A crash can cause a job to run again from fresh
context; generated tokens do not resume. See [SKILL.md](SKILL.md) for the
agent workflow and [INSTALL.md](../../INSTALL.md) for engine details.
