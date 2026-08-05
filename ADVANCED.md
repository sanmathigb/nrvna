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

```bash
MODEL=/path/to/model.gguf
WS=./workspace
GOAL="Write a cover letter for a senior engineer position"
nrvnad "$MODEL" "$WS" &
DAEMON_PID=$!

# First draft
draft_job=$(wrk "$WS" "$GOAL")
draft=$(flw "$WS" "$draft_job" -w)

# Critique
critique_job=$(wrk "$WS" "Critique this draft. What's weak? $draft")
critique=$(flw "$WS" "$critique_job" -w)

# Improve
final_job=$(wrk "$WS" "Improve this draft based on feedback:
Draft: $draft
Feedback: $critique")
final=$(flw "$WS" "$final_job" -w)

echo "$final"
nrvnad stop "$WS"
wait "$DAEMON_PID"
```

---

## Agent Loop

Iterate until done:

```bash
MODEL=/path/to/model.gguf
WS=./workspace
GOAL="Write a Python tutorial covering variables, loops, and functions"
memory=""
nrvnad "$MODEL" "$WS" &
DAEMON_PID=$!

for i in {1..5}; do
  job=$(wrk "$WS" "Goal: $GOAL
Previous work: $memory
Continue. Write the next section. Say DONE if complete.")
  result=$(flw "$WS" "$job" -w)

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

nrvnad stop "$WS"
wait "$DAEMON_PID"
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
