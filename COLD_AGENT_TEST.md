# Cold-agent test

Use this runbook to test whether a new agent can discover `imgsrch`, use an
existing index safely, and understand the nrvna primitives underneath it.
The test is about documentation and agent behavior, not model downloads.

Run the same test in each agent before changing the prompt. Using the same
model in multiple agents is useful: it isolates differences in the agent
harness, shell access, approvals, and instruction discovery.

Use two test tracks:

1. **One full-install canary per release and platform:** one agent downloads
   the public archive and models, creates a new project, indexes a small real
   corpus, searches it, and verifies the report. This tests the entire user
   journey and the installation documentation.
2. **Many reuse tests:** Claude, Codex, Pi, and OpenCode share the completed
   canary project and model cache. They test discovery, retrieval, evidence
   selection, and nrvna comprehension without repeating expensive work.

Do not make every agent download 3.4 GB and index the same images. That mostly
measures bandwidth and CPU time, not documentation quality.

## 1. Run one full-install canary

Do this once before comparing agents, and repeat it when the public package,
setup flow, model manifest, or supported platform changes. Use a new empty
model directory, a new project, and three to ten representative screenshots.
Do not use the source checkout or an existing installation: this test is for
the public release journey.

Start one cold agent in an empty directory with an isolated model cache:

```bash
mkdir -p /tmp/nrvna-cold/full-install
cd /tmp/nrvna-cold/full-install
IMGSRCH_MODELS_DIR="$HOME/imgsrch-cold-canary/models" claude
```

Paste:

```text
Set up imgsrch for me by following
https://raw.githubusercontent.com/sanmathigb/nrvna/main/apps/imgsrch/INSTALL.md
exactly. This is a full clean-install canary: you may download the public
release and its models. Use the IMGSRCH_MODELS_DIR already present in your
environment. Run the steps yourself, verify each gate, and ask me only where
the guide says to. Do not inspect an existing nrvna source checkout.

After I provide an image folder, create a new project under
$HOME/imgsrch-cold-canary/project, index it, and hand off as documented. Do not
poll background work. When I return after indexing completes, check status
once, run one useful search, inspect search-results.md, and open at most three
returned originals. Record the complete journey in AGENT_REPORT.md in your
current working directory.
```

This canary is not complete merely because setup succeeds. It must eventually
verify all of these transitions:

```text
archive -> doctor -> init -> index -> background completion -> search -> preview
```

Indexing may outlive the first agent session. Preserve the project path and
resume later; that durability is part of the test. Once complete, use this
project and model directory as the reusable assets in the remaining sections.

## 2. Check the reusable assets

Run this yourself before starting an agent. Do not paste the output into the
agent until it asks for paths.

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

- **`search: N indexed`, where `N > 0`:** a usable index exists. The agent may
  search it without downloading models or indexing images.
- **Initialized but `search: 0 indexed`:** no useful search corpus exists.
  Do not ask the agent to evaluate retrieval.
- **Project directory exists but either manifest is missing:** it is stale or
  incomplete, not an existing index. Do not reuse it.
- **Models ready but no usable index:** the model download can be skipped,
  but a future hands-on test still needs a new project and source images.
- **Binary missing:** use the current source-built binary above if present, or
  provide a packaged binary. Do not make a cold documentation test build it.

At the time this file was written, the model cache existed, but the old test
project was not a usable index: its manifests were missing. Re-run the check;
`/tmp` state is temporary.

## 3. Start each agent cold

Use a different empty working directory for every run. Do not start inside the
nrvna checkout: repository instructions and local files would make the test
warm rather than cold.

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

If a launcher depends on a runtime missing from its shebang, invoke it through
the runtime already used to install it. For example:

```bash
bun /usr/local/bin/codex
```

An empty directory removes repository context, not the agent's global config,
memories, or provider account. Use a fresh session and record any prior
knowledge the agent reveals.

## 4. Paste this first

Paste the same block into every agent:

```text
Explore https://github.com/sanmathigb/nrvna as a completely new user.

Start with the first useful application you discover. Explain what problem it
solves, the normal human journey, and how an agent could use it without loading
an entire image collection into context. Then explain what powers that
application and why it exists.

Do not install anything, download models, index images, run local binaries, or
inspect files elsewhere on this machine yet. Use only the public repository
documentation. Clearly separate documented facts from your own inferences.

Return a short onboarding report containing:
- agent, harness version, model if known, date, and working directory;
- token usage, wall time, tool calls, and images opened when exposed;
- the constraints you followed;
- what you discovered first and why;
- the commands you expect a human to use;
- the workflow you expect an agent to use;
- your understanding of imgsrch and nrvna;
- anything ambiguous, missing, or surprising;
- the smallest hands-on test you would run next.

Also write the report to AGENT_REPORT.md in your current empty working
directory. Do not write anywhere else on the machine.
```

This phase should reveal whether the repository naturally leads from a real
problem (`imgsrch`) to the underlying primitives (`wrk`, `nrvnad`, and `flw`).
Do not tell the agent that answer in advance.

Treat repository discovery and documentation comprehension as separate gates.
If the agent says the repository has no README, cannot read the landing page,
or starts inventing command syntax, preserve that report as a discovery
failure. Then continue the same session with this control prompt:

```text
The public repository does have a README. Your browsing path did not retrieve
it. Record that as a discovery failure, then read these public raw documents:

https://raw.githubusercontent.com/sanmathigb/nrvna/main/README.md
https://raw.githubusercontent.com/sanmathigb/nrvna/main/DOCUMENTATION.md
https://raw.githubusercontent.com/sanmathigb/nrvna/main/apps/imgsrch/README.md

Do not inspect local files or implementation code. Correct every unsupported
claim and placeholder command in AGENT_REPORT.md using only those documents.
Keep the original discovery failure in its own section so it is not erased.
```

If the control succeeds, the documentation is comprehensible but natural
repository discovery failed. If it still fails, record the agent harness's
public-web limitation; do not misclassify that as missing repository content.

For normal use, an agent should run `status` once and then `search`. It should
not run `eval` without a labeled hard set, use `add` when `index` is intended,
or assume imgsrch supports `--json`. Search has concise terminal output and a
rich `search-results.md`; JSON flags belong to the underlying nrvna commands.

## 5. If a usable index exists

After reading its report, provide only these paths:

The agent must have write access to the project. Search submits a query
embedding job and writes `search-results.md`; a read-only sandbox can inspect
status but cannot search. Codex CLI v0.144.6 may resume a prior session in
read-only mode unless `-s workspace-write` is supplied again. Record that as a
harness failure, not an imgsrch failure.

```text
An existing local test is available. Use only these paths:

binary: <IMGSRCH_BIN>
models: <IMGSRCH_MODELS>
project: <IMGSRCH_PROJECT>

Check status once. If at least one image is indexed, run one natural-language
search, inspect search-results.md, and assess whether the results help answer
the query. You may open at most three returned originals for verification.
Do not run setup, init, index, add, or eval. Do not enumerate the image folder.
Do not inspect other files on this machine. Report commands, outputs, evidence,
and any confusion separately. Update AGENT_REPORT.md with this hands-on phase.
```

Replace the angle-bracketed values with the paths from section 2. This tests
retrieval and agent judgment without repeating setup or indexing.

## 6. If no usable index exists

Do not pretend the stale directory is indexed. Keep the run read-only and ask:

```text
There is currently no reusable imgsrch index. Do not download models, create a
project, or index images. Based on the public documentation and your first
report, write the exact safe handoff you would give a user who already had a
completed index. Include how you would check status, search iteratively, limit
the originals opened, cite evidence, and stop when the index is not ready.

Then explain which parts are imgsrch product behavior and which parts are
nrvna primitive behavior. Do not invent context inheritance, dependency
execution, model routing, retries, or semantic artifact search in nrvna.
Update AGENT_REPORT.md with this analysis and state clearly that retrieval was
not executed.
```

This still tests comprehension. Create a small reusable index once, separately,
before running the hands-on phase across all agents. Do not make every cold
agent repeat expensive setup and indexing.

## 7. Optional nrvna smoke test

Only after the agent understands imgsrch, let it verify the substrate:

```text
Now run the smallest isolated nrvna smoke test documented by the repository.
Use a new workspace under /tmp, a compatible model path I explicitly provide,
and --drain. Submit work before starting the daemon, retrieve the result, and
report the job states, artifacts, exit codes, and what remained the caller's
responsibility. Make no repository changes and inspect no unrelated files.
Update AGENT_REPORT.md with the commands, exit codes, and observed artifacts.
```

Prefer an embedding model for a cheap deterministic lifecycle test. Provide
the model path only when asked. The agent should discover the documented
`wrk -> nrvnad --drain -> flw` contract itself.

## 8. Test retention and transfer

After the hands-on phases, keep the same agent session but prohibit further
reading and tool use. This is intentionally a warm, closed-book test: it
measures whether the agent retained nrvna's boundaries and can transfer them
to a new application. It does not measure cold discovery or originality.

```text
Without rereading documentation, inspecting code, browsing, or running tools,
choose one application other than image search that you would genuinely build
with the nrvna primitives. Do not give a wishlist.

Name the user and recurring problem. Show files in, model-role workspaces,
wrk submissions, nrvnad lifecycle, flw retrieval, application-owned
transitions, and files out. Separate what nrvna provides today from the
application code and future work. Explain failure, retry, duplicate execution,
context, validation, and why nrvna helps more than a direct model loop for this
specific workload. State what must remain outside nrvna core.

Clearly distinguish documented or observed behavior from inference. Do not
invent inherited context, dependency execution, model routing, retries, or
semantic search in nrvna.
```

Grade the architecture, not the novelty of the idea. A successful answer uses
the primitives as a small substrate, keeps product orchestration in the app,
and names where a direct script or conventional queue would be a better fit.

## 9. Required report

Each agent must leave `AGENT_REPORT.md` in its own cold-start directory. The
report is the test artifact, not a polished review. Require this structure:

```markdown
# nrvna cold-agent report

## Environment
Agent, harness version, model if known, date, working directory.

## Agent usage
Input, cached-input, output, and reasoning tokens if the harness exposes them;
wall time; tool calls; images opened. Write "not exposed" rather than estimate.

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

Do not grade prose quality. Grade whether claims are grounded, commands are
correct, constraints were respected, and the agent completed the intended
journey without hidden help.

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

Test different agent harnesses first. After fixing documentation failures,
repeat the hardest run with a smaller model. That separates documentation
quality from frontier-model reasoning ability.

## 11. Field record

Keep this table short. Add an entry only after an observed agent run changes a
document, command contract, or test. Raw transcripts may contain private local
paths and stay in the private session archive; this is the versioned public
record of what the incident changed.

| Observed incident | What it established | Enforced response |
| --- | --- | --- |
| A cold Codex session failed to retrieve the repository README, then invented placeholder commands. The same session understood nrvna after receiving exact raw-document URLs. | Repository discovery and documentation comprehension are separate gates. | Section 4 preserves the discovery failure and runs a raw-document control instead of rewriting good documentation to fix a harness limitation. |
| A resumed Codex session could read an existing imgsrch project but search failed before inference because the sandbox was read-only. | Search is not read-only: it submits a query embedding and updates `search-results.md`. | Section 5 and the [imgsrch README](apps/imgsrch/README.md) require project write access and classify sandbox denial as a harness failure. |
| A completed three-image canary survived the initiating session, reached three searchable items, and returned the meeting-transcription screenshot first. | The packaged `doctor -> init -> index -> background completion -> search -> preview` journey works with cached, checksum-matched models. | Section 1 requires that full transition rather than treating setup alone as success. |
| A two-job embedding audit queued work before a daemon, drained with two workers, and retrieved two 768-dimensional artifacts. `flw -W --tag ... --json` exited `0` with empty stdout; plain tagged retrieval emitted NDJSON. | The bounded primitive path works, and a batch barrier is not result collection. | `flw --help`, `AGENTS.md`, `QUICKSTART.md`, and `tests/primitive-contract.sh` now state and enforce the distinction. |
| The same trained session designed a separate meeting-inbox application without rereading docs and kept routing, validation, retries, and dependencies outside core. | The agent retained and transferred the foundation after hands-on use. It did not prove cold originality. | Section 8 labels this a warm, closed-book transfer test and grades architectural boundaries rather than novelty. |
