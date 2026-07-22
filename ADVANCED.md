# Advanced Patterns

The primitives (`nrvnad`, `wrk`, `flw`) are building blocks. Here's what you can build with them.

---

## Batch Processing

Submit many jobs, collect results later:

```bash
# Submit 100 jobs in seconds
for img in photos/*.jpg; do
  wrk ./workspace "Caption this image" --image "$img" >> jobs.txt
done

# Check progress
ls ./workspace/output/ | wc -l

# Collect all results
for job in $(cat jobs.txt); do
  echo "=== $job ==="
  flw ./workspace $job
done
```

---

## Fan-Out / Fan-In (Map-Reduce)

Break a document into parallel subtasks, synthesize:

```bash
# Fan-out: summarize each chapter independently
job1=$({ echo "Summarize the key claims:"; cat report/ch1.txt; } | wrk ./workspace -)
job2=$({ echo "Summarize the key claims:"; cat report/ch2.txt; } | wrk ./workspace -)
job3=$({ echo "Summarize the key claims:"; cat report/ch3.txt; } | wrk ./workspace -)

# Wait for all
result1=$(flw ./workspace -w $job1)
result2=$(flw ./workspace -w $job2)
result3=$(flw ./workspace -w $job3)

# Fan-in: synthesize
wrk ./workspace "Merge these chapter summaries into one digest.
Preserve exact terms and numbers. Do not add new facts.
$result1
$result2
$result3"
```

The document never fit in one context window. It didn't need to — the window
is the step size, not the ceiling.

---

## Self-Refinement Loop

Generate, critique, improve:

```bash
GOAL="Write a cover letter for a senior engineer position"

# First draft
draft=$(wrk ./workspace "$GOAL" | xargs flw ./workspace -w)

# Critique
critique=$(wrk ./workspace "Critique this draft. What's weak? $draft" | xargs flw ./workspace -w)

# Improve
final=$(wrk ./workspace "Improve this draft based on feedback:
Draft: $draft
Feedback: $critique" | xargs flw ./workspace -w)

echo "$final"
```

---

## Agent Loop

Iterate until done:

```bash
GOAL="Write a Python tutorial covering variables, loops, and functions"
memory=""

for i in {1..5}; do
  result=$(wrk ./workspace "Goal: $GOAL
Previous work: $memory
Continue. Write the next section. Say DONE if complete." | xargs flw ./workspace -w)

  echo "=== Iteration $i ==="
  echo "$result"

  if echo "$result" | grep -q "DONE"; then
    break
  fi

  memory="$memory\n---\n$result"
done
```

---

## Vision Batch

Caption or analyze a directory of images:

```bash
nrvnad qwen-vl.gguf ./ws-vision    # mmproj auto-detected

for img in photos/*.jpg; do
  wrk ./ws-vision "Describe this image in detail" --image "$img" >> jobs.txt
done

# Collect all captions
for job in $(cat jobs.txt); do
  echo "=== $job ==="
  flw ./ws-vision -w $job
done
```

---

## Embeddings for Search

Generate embeddings and use them for similarity:

```bash
# Generate embeddings for a corpus
for doc in docs/*.txt; do
  content=$(cat "$doc")
  job=$(wrk ./workspace "$content" --embed)
  echo "$doc $job" >> embed-jobs.txt
done

# Results are JSON files in output/<job-id>/embedding.json
# Each contains: { "dim": N, "vector": [...] }
```

---

## Text-to-Speech

Generate audio from text:

```bash
nrvnad outetts.gguf ./ws-tts    # vocoder auto-detected

job=$(cat article-intro.txt | wrk ./ws-tts - --tts)
flw ./ws-tts -w $job

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

# Terminal 2: Submit jobs
for f in inbox/*.txt; do
  { echo "Summarize:"; cat "$f"; } | wrk ./workspace -
done
```

---

## Memory / Context Accumulation

Build up knowledge across jobs:

```bash
# Save results to memory
flw ./workspace $job1 >> memory.txt
flw ./workspace $job2 >> memory.txt
flw ./workspace $job3 >> memory.txt

# Use memory as context
wrk ./workspace "Given these earlier findings:
$(cat memory.txt)

What themes appear in all of them? List contradictions separately."
```

---

## Multi-Model Routing

Different models for different tasks:

```bash
# Start specialized daemons
nrvnad qwen-vl.gguf    ./ws-vision &    # mmproj auto-detected
nrvnad codellama.gguf  ./ws-code   &
nrvnad phi-3-mini.gguf ./ws-fast   &
nrvnad outetts.gguf    ./ws-tts    &    # vocoder auto-detected

# Route by task type
classify() {
  case "$1" in
    *.jpg|*.png) echo "./ws-vision" ;;
    *.py|*.js)   echo "./ws-code" ;;
    *)           echo "./ws-fast" ;;
  esac
}

# Submit to appropriate workspace
ws=$(classify "$input")
wrk "$ws" "Process this: $input"
```

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `NRVNA_WORKERS` | `4` | Worker threads |
| `NRVNA_PREDICT` | `2048` | Max tokens to generate |
| `NRVNA_MAX_CTX` | `8192` | Context window size |
| `NRVNA_BATCH` | `2048` | Batch size |
| `NRVNA_TEMP` | GGUF hint or `0.8` | Sampling temperature |
| `NRVNA_VISION_TEMP` | `0.3` | Vision sampling temperature |
| `NRVNA_TOP_K` | GGUF hint or `40` | Top-K sampling |
| `NRVNA_TOP_P` | GGUF hint or `0.9` | Top-P (nucleus) sampling |
| `NRVNA_MIN_P` | GGUF hint or `0.05` | Min-P sampling |
| `NRVNA_REPEAT_PENALTY` | GGUF hint or `1.1` | Repetition penalty |
| `NRVNA_GPU_LAYERS` | `0` | Layers offloaded to GPU (default CPU; set >0 to offload, e.g. `99` on Apple Silicon) |
| `NRVNA_UBATCH` | `NRVNA_BATCH` | Physical batch size; lower it when memory is constrained |
| `NRVNA_THINKING` | enabled | Set `0` to disable thinking in templates that support it |
| `NRVNA_MODELS_DIR` | `./models/` | Model search path |
| `NRVNA_LOG_LEVEL` | `info` | Logging: error, warn, info, debug, trace |

---

## Tips

1. **More workers = more parallelism** — but diminishing returns past CPU cores
2. **Vision is serialized** — mutex prevents parallel vision encoding corruption
3. **Jobs are directories** — inspect with `ls`, `cat`, `tree`
4. **Atomic state** — job location *is* job state
5. **Compose with shell** — the primitives are designed for piping
6. **TTS vocoder auto-detected** — place vocoder .gguf next to your OuteTTS model
