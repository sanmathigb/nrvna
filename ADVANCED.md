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

## Self-Refinement Loop

Generate a draft. Critique it. Then improve it.

The source-tree helper starts one daemon or uses one that is already starting.
It waits for readiness and reports startup failures from the daemon log.

```bash
source ./scripts/nrvna-lib.sh

self_refine() (
  set -e
  local model=/path/to/model.gguf
  local ws=./workspace
  local goal="Write a cover letter for a senior engineer position"
  trap 'nrvna_stop "$ws" >/dev/null 2>&1 || true' EXIT
  nrvna_start "$model" "$ws"

  # First draft
  draft_job=$(wrk "$ws" "$goal")
  draft=$(flw "$ws" "$draft_job" -w)

  # Critique
  critique_job=$(wrk "$ws" "Critique this draft. What's weak? $draft")
  critique=$(flw "$ws" "$critique_job" -w)

  # Improve
  final_job=$(wrk "$ws" "Improve this draft based on feedback:
Draft: $draft
Feedback: $critique")
  final=$(flw "$ws" "$final_job" -w)

  echo "$final"
  nrvna_stop "$ws"
  trap - EXIT
)

self_refine
```

---

## Agent Loop

Iterate until done:

```bash
source ./scripts/nrvna-lib.sh

write_tutorial() (
  set -e
  local model=/path/to/model.gguf
  local ws=./workspace
  local goal="Write a Python tutorial covering variables, loops, and functions"
  local memory=""
  trap 'nrvna_stop "$ws" >/dev/null 2>&1 || true' EXIT
  nrvna_start "$model" "$ws"

  for i in {1..5}; do
    job=$(wrk "$ws" "Goal: $goal
Previous work: $memory
Continue. Write the next section. Say DONE if complete.")
    result=$(flw "$ws" "$job" -w)

    echo "=== Iteration $i ==="
    echo "$result"

    if echo "$result" | grep -q "DONE"; then
      break
    fi

    if [ -z "$memory" ]; then
      memory="$result"
    else
      memory=$(printf '%s\n---\n%s' "$memory" "$result")
    fi
  done

  nrvna_stop "$ws"
  trap - EXIT
)

write_tutorial
```

---

## Vision Batch

Caption or analyze each image in a directory:

```bash
: > jobs.txt

for img in photos/*.jpg; do
  wrk ./ws-vision "Describe this image in detail" --image "$img" >> jobs.txt
done

# Load once, drain the image jobs, and release the model
nrvnad qwen-vl.gguf ./ws-vision --drain    # mmproj auto-detected

# Collect all captions
while IFS= read -r job; do
  echo "=== $job ==="
  flw ./ws-vision "$job"
done < jobs.txt
```

---

## Embeddings for Search

Generate embeddings for similarity search:

```bash
: > embed-jobs.txt

# Generate embeddings for a corpus
for doc in docs/*.txt; do
  content=$(cat "$doc")
  job=$(wrk ./workspace "$content" --embed)
  echo "$doc $job" >> embed-jobs.txt
done

# Process the queued work
nrvnad embedding-model.gguf ./workspace --drain

# Results are JSON files in output/<job-id>/embedding.json
# Each contains: { "dim": N, "vector": [...] }
```

---

## Text-to-Speech

Generate audio from text:

```bash
job=$(cat article-intro.txt | wrk ./ws-tts - --tts)
nrvnad outetts.gguf ./ws-tts --drain    # vocoder auto-detected
flw ./ws-tts "$job"

# Result is a WAV file at workspace/output/<job-id>/audio.wav
# Keep each job to a few sentences; chunk longer text into multiple jobs
```

---

## Event-Driven (Watch for Results)

Monitor completions with `fswatch`:

```bash
# Terminal 1: Watch for results
fswatch -0 ./workspace/output | while read -d '' path; do
  [[ "$path" == */result.txt ]] && cat "$path"
done

# Terminal 2: Keep the model ready. This command stays open.
nrvnad text-model.gguf ./workspace

# Terminal 3: Submit jobs
for f in inbox/*.txt; do
  { echo "Summarize:"; cat "$f"; } | wrk ./workspace -
done
```

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
