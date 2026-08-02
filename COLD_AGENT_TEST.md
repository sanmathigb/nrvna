# Cold-agent test

Use this runbook to test a new agent. The agent must discover `imgsrch`, use an
existing index safely, and understand nrvna. This test measures documentation
and agent behavior. It does not measure model downloads.

Run the same test in each agent before you change the prompt. You can use the
same model in multiple agents. This isolates differences in the harness, shell
access, approvals, and instruction discovery.

Use two test tracks:

1. **Run one full-install canary for each release and platform.** One agent
   downloads the archive and models. It creates a project and indexes a small
   real corpus. It then searches the corpus and verifies the report.
2. **Run many reuse tests.** Claude, Codex, Pi, and OpenCode share the completed
   project and model cache. They test discovery, retrieval, evidence selection,
   and nrvna comprehension.

Do not make every agent download 3.4 GB or index the same images. Those steps
measure bandwidth and CPU time. They do not measure documentation quality.

## 1. Run one full-install canary

Run this test once before you compare agents. Repeat it after a package, setup,
model manifest, or platform change. Use an empty model directory and a new
project. Use three to ten representative screenshots. Do not use the source
checkout or an existing installation.

Start one cold agent in an empty directory with an isolated model cache:

```bash
mkdir -p /tmp/nrvna-cold/full-install
cd /tmp/nrvna-cold/full-install
IMGSRCH_MODELS_DIR="$HOME/imgsrch-cold-canary/models" claude
```

Paste:

```text
Set up imgsrch. Follow
https://raw.githubusercontent.com/sanmathigb/nrvna/main/apps/imgsrch/INSTALL.md
exactly. This is a clean-install test. You may download the public release and
its models. Use the existing IMGSRCH_MODELS_DIR value. Run and verify each
step. Ask a question only when the guide tells you. Do not inspect a local
nrvna source checkout.

After I provide an image folder, create a project under
$HOME/imgsrch-cold-canary/project. Index the images. Complete the documented
handoff. Do not poll background work. When I return, check status once. If
indexing is complete, run one useful search. Inspect search-results.md. Open at
most three returned originals. Record the journey in AGENT_REPORT.md in the
current directory.
```

Setup alone does not complete this test. The test must verify these
transitions:

```text
archive -> doctor -> init -> index -> background completion -> search -> preview
```

Indexing can outlive the first agent session. Preserve the project path and
resume later. This durability is part of the test. Reuse the completed project
and model directory in later tests.

## 2. Check the reusable assets

Run this check before you start an agent. Do not give the output to the agent
until it asks for paths.

```bash
IMGSRCH_BIN="$HOME/ws/nrvna/apps/imgsrch/imgsrch"
IMGSRCH_MODELS=/tmp/imgsrch-fresh/models
IMGSRCH_PROJECT=/tmp/imgsrch-fresh/test-project

test -x "$IMGSRCH_BIN" && echo "binary: ready" || echo "binary: missing"
models_ready=true
for model in \
  LFM2.5-VL-1.6B-Q8_0.gguf \
  mmproj-LFM2.5-VL-1.6b-Q8_0.gguf \
  GLM-OCR-Q8_0.gguf \
  mmproj-GLM-OCR-Q8_0.gguf \
  nomic-embed-text-v1.5.Q8_0.gguf; do
  test -f "$IMGSRCH_MODELS/$model" || models_ready=false
done
$models_ready && echo "models: ready" || echo "models: missing or incomplete"
test -f "$IMGSRCH_PROJECT/.imgsrch/items.tsv" \
  && test -f "$IMGSRCH_PROJECT/.imgsrch/index/index.tsv" \
  && echo "project: initialized" \
  || echo "project: not initialized"

if test -x "$IMGSRCH_BIN" \
  && test -f "$IMGSRCH_PROJECT/.imgsrch/items.tsv" \
  && test -f "$IMGSRCH_PROJECT/.imgsrch/index/index.tsv"; then
  IMGSRCH_MODELS_DIR="$IMGSRCH_MODELS" \
    "$IMGSRCH_BIN" status "$IMGSRCH_PROJECT"
fi
```

Interpret the result strictly:

- **`search: N indexed`, where `N > 0`:** The index is usable. The agent can
  search without another download or index run.
- **Initialized but `search: 0 indexed`:** The project has no searchable
  corpus. Do not test retrieval.
- **A manifest is missing:** The project is stale or incomplete. Do not reuse
  it.
- **Models exist but no index exists:** Skip the model download. A later test
  still needs a project and source images.
- **The binary is missing:** Use the source-built binary when available.
  Otherwise, provide a packaged binary. Do not make a documentation test build
  it.

When this file was written, the model cache existed. The old project was not
usable because its manifests were missing. Always run the check again. `/tmp`
state is temporary.

## 3. Start each agent cold

Use a different empty directory for every run. Do not start inside the nrvna
checkout. Repository instructions and local files would make the test warm.

```bash
mkdir -p /tmp/nrvna-cold/{claude,codex,pi,opencode}
```

Start one agent per terminal:

```bash
cd /tmp/nrvna-cold/claude && claude
cd /tmp/nrvna-cold/codex && codex
cd /tmp/nrvna-cold/pi && pi
cd /tmp/nrvna-cold/opencode && opencode
```

If a launcher cannot find its runtime, invoke it through the installation
runtime. For example:

```bash
bun /usr/local/bin/codex
```

An empty directory removes repository context. It does not remove global
configuration, memory, or the provider account. Use a fresh session. Record
any prior knowledge that the agent reveals.

## 4. Paste this first

Paste the same block into every agent:

```text
Explore https://github.com/sanmathigb/nrvna as a new user.

Start with the first useful application that you discover. Explain its problem
and normal human journey. Explain how an agent can avoid loading the full image
collection into context. Then explain what powers the application and why that
system exists.

Do not install software or download models. Do not index images or run local
binaries. Do not inspect other local files. Use only public repository
documentation. Separate documented facts from your inferences.

Return a short onboarding report. Include:
- agent, harness version, model if known, date, and working directory;
- token usage, wall time, tool calls, and images opened when exposed;
- the constraints you followed;
- what you discovered first and why;
- the commands you expect a human to use;
- the workflow you expect an agent to use;
- your understanding of imgsrch and nrvna;
- anything ambiguous, missing, or surprising;
- the smallest hands-on test you would run next.

Write the report to AGENT_REPORT.md in the current empty directory. Do not
write elsewhere on the machine.
```

This phase tests the repository path from `imgsrch` to `wrk`, `nrvnad`, and
`flw`. Do not give that path to the agent.

Test repository discovery and document comprehension as separate gates. A
missing README report is a discovery failure. Invented commands are also a
discovery failure. Preserve the report. Then use this control prompt in the
same session:

```text
The public repository has a README. Your browsing path did not retrieve it.
Record this discovery failure. Then read these public raw documents:

https://raw.githubusercontent.com/sanmathigb/nrvna/main/README.md
https://raw.githubusercontent.com/sanmathigb/nrvna/main/DOCUMENTATION.md
https://raw.githubusercontent.com/sanmathigb/nrvna/main/apps/imgsrch/README.md

Do not inspect local files or implementation code. Use only these documents.
Correct each unsupported claim and placeholder command in AGENT_REPORT.md.
Keep the original discovery failure in a separate section.
```

If the control succeeds, the documents are clear but repository discovery
failed. If it fails, record the harness web limit. Do not report missing
repository content.

For normal use, run `status` once and then run `search`. Do not run `eval`
without a labeled hard set. Use `index`, not `add`, for normal ingestion.
imgsrch does not support `--json`. Search prints concise output and writes
`search-results.md`. JSON flags belong to nrvna commands.

## 5. If a usable index exists

After reading its report, provide only these paths:

The agent needs project write access. Search submits a query embedding job and
writes `search-results.md`. A read-only sandbox can inspect status but cannot
search. Codex CLI v0.144.6 can resume in read-only mode. Supply
`-s workspace-write` again. Report sandbox denial as a harness failure.

```text
An existing local test is available. Use only these paths:

binary: <IMGSRCH_BIN>
models: <IMGSRCH_MODELS>
project: <IMGSRCH_PROJECT>

Check status once. If an image is indexed, run one natural-language search.
Inspect search-results.md. Decide whether the results help answer the query.
Open at most three returned originals for verification. Do not run setup, init,
index, add, or eval. Do not list the image folder. Do not inspect other local
files. Report commands, output, evidence, and confusion separately. Update
AGENT_REPORT.md with this phase.
```

Replace the angle-bracketed values with paths from section 2. This tests
retrieval and judgment without another setup or index run.

## 6. If no usable index exists

Do not report a stale directory as indexed. Keep the run read-only. Use this
prompt:

```text
There is no reusable imgsrch index. Do not download models, create a project,
or index images. Use the public documentation and your first report. Write the
exact handoff for a user with a completed index. Include one status check and
iterative search. Limit the original images that you open. Cite evidence. Stop
when the index is not ready.

Separate imgsrch behavior from nrvna behavior. Do not invent inherited context,
dependency execution, model routing, retries, or semantic artifact search.
Update AGENT_REPORT.md. State that you did not execute retrieval.
```

This still tests comprehension. Create one small reusable index before the
hands-on tests. Do not make each cold agent repeat setup and indexing.

## 7. Optional nrvna smoke test

Run this test only after the agent understands imgsrch:

```text
Run the smallest documented nrvna test. Use a new workspace under /tmp. Use the
compatible model path that I provide. Use --drain. Submit work before you start
the daemon. Retrieve the result. Report job states, artifacts, and exit codes.
Report the caller's remaining responsibilities. Do not change the repository.
Do not inspect unrelated files. Update AGENT_REPORT.md with the observed
commands and results.
```

Prefer an embedding model for a small deterministic lifecycle test. Give the
model path only when the agent asks. The agent must discover the documented
`wrk -> nrvnad --drain -> flw` contract.

## 8. Test retention and transfer

After the hands-on phases, keep the same agent session. Prohibit more reading
and tool use. This warm, closed-book test measures retained boundaries and
transfer to a new application. It does not measure cold discovery or
originality.

```text
Do not read documentation, inspect code, browse, or run tools. Choose one
application that you would build with nrvna. Do not choose image search. Do
not give a feature list.

Name the user and recurring problem. Show input files, model workspaces, `wrk`
submissions, `nrvnad` lifecycle, and `flw` retrieval. Show application-owned
transitions and output files. Separate current nrvna behavior from application
code and future work. Explain failure, retry, duplicate execution, context,
and validation. Compare nrvna with a direct model loop for this workload. State
what must stay outside nrvna core.

Separate documented or observed behavior from inference. Do not invent
inherited context, dependencies, model routing, retries, or semantic search.
```

Grade the architecture, not the idea's novelty. A successful answer keeps
product orchestration in the application. It also identifies when a script or
conventional queue is a better choice.

## 9. Required report

Each agent must create `AGENT_REPORT.md` in its cold-start directory. The report
is test evidence. It is not a polished review. Use this structure:

```markdown
# nrvna cold-agent report

## Environment
Agent, harness version, model if known, date, working directory.

## Agent usage
Record input, cached input, output, and reasoning tokens when available. Record
wall time, tool calls, and images opened. Write "not exposed" when necessary.

## Constraints followed
What the agent was and was not allowed to inspect, download, or execute.

## Discovery path
Pages read in order and what led to imgsrch and nrvna.

## Documented understanding
What imgsrch does; what wrk, nrvnad, and flw do; lifecycle and boundaries.

## Inferences
Ideas not explicitly promised by the documentation.

## Commands and outcomes
Exact commands, exit codes, important output, project status, and artifacts.
Write "not executed" for phases that were intentionally skipped.

## Retrieval evidence
Query, ranked paths, originals opened, usefulness, and cited filenames.

## Friction and ambiguity
Every hesitation, unnecessary question, failed command, or misleading phrase.

## Verdict
Could a new user or agent succeed? What single documentation change would
help most?
```

Do not grade prose quality. Grade evidence, command accuracy, constraint
compliance, and task completion. Do not give hidden help.

## 10. Compare the runs

Record evidence, not general impressions:

| Question | Claude | Codex | Pi | OpenCode |
| --- | --- | --- | --- | --- |
| Found imgsrch without prompting? | | | | |
| Found the agent install path? | | | | |
| Explained the human journey correctly? | | | | |
| Understood local/private boundaries? | | | | |
| Distinguished imgsrch from nrvna? | | | | |
| Understood fresh context and lineage-only parents? | | | | |
| Transferred the primitives without growing core? | | | | |
| Respected the no-download/no-index constraint? | | | | |
| Avoided enumerating or opening the full corpus? | | | | |
| Used status once and handled zero indexed honestly? | | | | |
| Produced correct runnable commands? | | | | |
| Reported tokens, wall time, tool calls, and images opened? | | | | |
| Hallucinations or unnecessary questions | | | | |

Test different agent harnesses first. Fix observed documentation failures.
Then repeat the hardest test with a smaller model. This separates document
quality from model reasoning ability.

## 11. Field record

Keep this table short. Add an entry only when an observed run changes a
document, command contract, or test. Raw transcripts can contain private local
paths. Keep them in the private session archive. This table is the public
record of each resulting change.

| Observed incident | What it established | Enforced response |
| --- | --- | --- |
| A cold Codex session failed to retrieve the repository README, then invented placeholder commands. The same session understood nrvna after receiving exact raw-document URLs. | Repository discovery and documentation comprehension are separate gates. | Section 4 preserves the discovery failure and runs a raw-document control instead of rewriting good documentation to fix a harness limitation. |
| A resumed Codex session could read an existing imgsrch project but search failed before inference because the sandbox was read-only. | Search is not read-only: it submits a query embedding and updates `search-results.md`. | Section 5 and the [imgsrch README](apps/imgsrch/README.md) require project write access and classify sandbox denial as a harness failure. |
| A completed three-image canary survived the initiating session, reached three searchable items, and returned the meeting-transcription screenshot first. | The packaged `doctor -> init -> index -> background completion -> search -> preview` journey works with cached, checksum-matched models. | Section 1 requires that full transition rather than treating setup alone as success. |
| A two-job embedding audit queued work before a daemon, drained with two workers, and retrieved two 768-dimensional artifacts. `flw -W --tag ... --json` exited `0` with empty stdout; plain tagged retrieval emitted NDJSON. | The bounded primitive path works, and a batch barrier is not result collection. | `flw --help`, `AGENTS.md`, `QUICKSTART.md`, and `tests/primitive-contract.sh` now state and enforce the distinction. |
| The same trained session designed a separate meeting-inbox application without rereading docs and kept routing, validation, retries, and dependencies outside core. | The agent retained and transferred the foundation after hands-on use. It did not prove cold originality. | Section 8 labels this a warm, closed-book transfer test and grades architectural boundaries rather than novelty. |
