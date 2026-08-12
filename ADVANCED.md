# Advanced Patterns

Use `nrvnad`, `wrk`, and `flw` to build the following patterns.

---

## Batch Processing

Submit many independent jobs before loading the model, then wait for and
collect only that batch:

```bash
BATCH="captions-$(date +%s)"
: > jobs.txt

for img in photos/*.jpg; do
  wrk ./workspace "Caption this image" --image "$img" --tag "$BATCH" \
    >> jobs.txt
done

# Load once, drain the queued work, and release the model
nrvnad vision-model.gguf ./workspace --drain

# Drain has already waited; collect this batch as NDJSON
flw ./workspace --tag "$BATCH" --json > results.ndjson
```

Tags group jobs only. They do not share context or control execution. Keep
`jobs.txt` when you need the source-to-job mapping. If a daemon owns the
workspace, run `flw ./workspace -W --tag "$BATCH"` as a silent barrier. Then
run the collection command.

---

## Fan-Out and Fan-In

Fan-out sends parts to independent jobs. Fan-in combines their results.

```bash
MODEL=/path/to/model.gguf

# Fan-out: summarize each chapter independently
job1=$({ echo "Summarize the key claims:"; cat report/ch1.txt; } | wrk ./workspace -)
job2=$({ echo "Summarize the key claims:"; cat report/ch2.txt; } | wrk ./workspace -)
job3=$({ echo "Summarize the key claims:"; cat report/ch3.txt; } | wrk ./workspace -)

# Load once and finish the queued summaries
nrvnad "$MODEL" ./workspace --drain
result1=$(flw ./workspace "$job1")
result2=$(flw ./workspace "$job2")
result3=$(flw ./workspace "$job3")

# Fan-in: synthesize
final_job=$(wrk ./workspace "Merge these chapter summaries into one digest.
Preserve exact terms and numbers. Do not add new facts.
$result1
$result2
$result3")
nrvnad "$MODEL" ./workspace --drain
flw ./workspace "$final_job"
```

Each job uses one bounded context. The final job receives only the selected
results.

---

## Persistent Daemon

Use the source-tree helper when a shell application needs a ready daemon. The
helper starts one daemon or adopts one that is already starting. It waits for
readiness and reports startup failures.

```bash
source ./scripts/nrvna-lib.sh

persistent_job() (
  set -e
  model=/path/to/model.gguf
  workspace=./workspace
  owns_daemon=false
  status_code=0
  nrvna_status "$workspace" || status_code=$?
  [ "$status_code" -eq 1 ] && owns_daemon=true

  cleanup() {
    [ "$owns_daemon" = true ] || return 0
    nrvna_stop "$workspace" || {
      echo "persistent_job: failed to stop the daemon" >&2
      return 1
    }
  }
  trap cleanup EXIT

  nrvna_start "$model" "$workspace"
  job=$(wrk "$workspace" "Reply with exactly: ready")
  flw "$workspace" -w "$job"
)

persistent_job
```

This helper is optional. It does not add another nrvna primitive.

---

## Explicit Context

Save results and include them in a later prompt:

```bash
MODEL=/path/to/model.gguf

first=$(wrk ./workspace "Extract the important facts")
nrvnad "$MODEL" ./workspace --drain

# Put the prior result in the next prompt
second=$({
  echo "Given these earlier findings:"
  flw ./workspace "$first"
  echo
  echo "What themes appear? List contradictions separately."
} | wrk ./workspace - --parent "$first")

nrvnad "$MODEL" ./workspace --drain
flw ./workspace "$second"
```

---

## One Model, One Workspace, One Drain

Use a separate workspace for each model role. Drain them in sequence when the
models cannot share memory:

```bash
vision_job=$(wrk ./ws-vision "Describe this image" --image screenshot.png)
code_job=$({ echo "Review this code:"; cat app.py; } | wrk ./ws-code -)
text_job=$(wrk ./ws-fast "Classify this request: reset my password")

nrvnad qwen-vl.gguf    ./ws-vision --drain    # mmproj auto-detected
nrvnad codellama.gguf  ./ws-code   --drain
nrvnad phi-3-mini.gguf ./ws-fast   --drain

flw ./ws-vision "$vision_job"
flw ./ws-code "$code_job"
flw ./ws-fast "$text_job"
```

---

## Operational Notes

1. More workers increase parallel work. Extra workers can reduce performance
   after all CPU cores are busy.
2. nrvna serializes vision encoding. This prevents corruption in shared GGML
   compute state.
3. Jobs are directories. Inspect them with `ls`, `cat`, or `tree`.
4. A job's directory defines its state.
5. The commands support shell pipes.
6. Place the vocoder GGUF beside the OuteTTS model. `nrvnad` detects it.
