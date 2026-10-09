# Verified file organizer

This example separates model output from file operations.

LFM2.5-2.6B proposes a JSON move plan. nrvna preserves the job, schema, and
result. `file-organizer-validator` checks the plan against the inbox. It moves
files only when you use `--apply`.

Build the example target from the repository root:

```bash
cmake --build build --target file-organizer-validator wrk nrvnad flw
```

Make a disposable copy. Set `MODEL` to a compatible GGUF file.

```bash
DEMO=$(mktemp -d /tmp/nrvna-file-organizer.XXXXXX)
cp -R examples/file-organizer/. "$DEMO/"
cd "$DEMO"
PATH=/path/to/nrvna/build:$PATH
MODEL=/path/to/LFM2.5-2.6B-Q4_K_M.gguf
```

Submit, run, and read one durable inference job:

```bash
job=$(wrk ./workspace - \
  --json-schema move-plan.schema.json \
  < request.txt)

nrvnad "$MODEL" ./workspace --drain
flw ./workspace "$job" > plan.json
```

Check every operation before one file moves:

```bash
file-organizer-validator ./inbox plan.json Release Encoders
file-organizer-validator --apply ./inbox plan.json Release Encoders
```

The first call is a dry run. The second call moves four files. It leaves two
review files in the inbox.

The validator accepts direct file names only. It rejects traversal, unknown
categories, duplicate entries, omitted files, symlinks, and collisions. It
checks the full plan before it moves a file. You can repeat `--apply` after an
interruption. The validator recognizes files already at their planned paths.

nrvna validates the JSON format. It does not validate file-operation meaning.
The caller owns that check.

## Recorded compatibility check

The local canary and terminal recording used this model:

- Source: [LiquidAI/LFM2.5-2.6B-GGUF at `b22e29ebf6249a8c9fcdda36914743e9980595c4`](https://huggingface.co/LiquidAI/LFM2.5-2.6B-GGUF/blob/b22e29ebf6249a8c9fcdda36914743e9980595c4/LFM2.5-2.6B-Q4_K_M.gguf)
- File: `LFM2.5-2.6B-Q4_K_M.gguf`
- Quantization: Q4_K_M
- SHA-256: `79fdf00351b46cf26f020aead28d01889886be87c55fa0eb907e6f9b00bfee14`
- Machine: MacBookPro14,3, Intel Core i7 3.1 GHz, 16 GB RAM
- System: macOS 13.7.5, x86_64, CPU inference

The observed plan matched [`expected-plan.json`](expected-plan.json). The dry
run changed nothing. Apply created `Release/` and `Encoders/` and moved the
four selected files. `team-lunch.txt` and `scratchpad.md` stayed in place.

This is a compatibility check. It is not a benchmark.
