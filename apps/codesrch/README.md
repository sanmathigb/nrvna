# codesrch

`codesrch` finds code by meaning. It uses nrvna for embeddings and answers.
Each embedding and answer is a separate durable nrvna job.

Build it from the nrvna repository:

```bash
cmake -S apps/codesrch -B build/codesrch
cmake --build build/codesrch -j4
```

Index a Git repository and search it:

```bash
build/codesrch/codesrch init .
build/codesrch/codesrch index .
build/codesrch/codesrch search . "where is POST authentication checked?"
build/codesrch/codesrch ask . "where is POST authentication checked?"
```

Search combines semantic similarity with keyword ranking through reciprocal
rank fusion. It prints ranked file paths, line ranges, scores, and source snippets.
`ask` sends the best matches to a local text model and prints a cited answer.

The app reads Git-tracked and unignored files. It skips binary files, files
over 1 MiB, and common generated directories. It uses chunks of up to 40 lines,
with an 8-line overlap and a 1,500-byte limit. Long chunks split into smaller
parts. Re-indexing reuses exact matching chunks and removes deleted chunks from
the active index.

The project data lives in `.codesrch/`. The current index stores each compact
reference as a six-byte fingerprint and five-byte vector offset. Vectors and
source metadata are stored separately. The 11-byte figure describes only the
reference table, not the full index.

The default embedding model is `nomic-embed-text-v1.5.Q8_0.gguf`. The default
answer model is `Qwen 3.5 4B Q4 K S.gguf`. Models resolve from `models/` or
`NRVNA_MODELS_DIR`. Set `NRVNA_CODE_EMBED_MODEL` or
`NRVNA_CODE_ANSWER_MODEL` to use another model file.

Set `NRVNA_WRK`, `NRVNA_NRVNAD`, or `NRVNA_FLW` to select nrvna executables.
Otherwise, `codesrch` checks `PATH` and then `./build` in the nrvna source tree.
It uses `--drain` when no daemon owns its workspace. If a daemon is running,
it submits work and waits for that batch.

Run the model-free unit tests:

```bash
ctest --test-dir build/codesrch --output-on-failure
```

This prototype uses exact cosine and BM25 ranking. It does not use an
approximate nearest-neighbor index or prove that the compact reference table
reduces total memory. Compare relevance and memory use with qmd before making
claims.
