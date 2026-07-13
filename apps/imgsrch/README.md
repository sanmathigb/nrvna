# imgsrch

`imgsrch` is a native local tool for creating searchable image projects.
It packages a small Go CLI over nrvna and llama.cpp. The user operates one
command and does not manage the backend.

## Install

Download and extract the release archive for your platform. Keep `imgsrch` and
its bundled `bin/` directory together. The release contains everything except
models.

```bash
tar -xzf imgsrch-darwin-arm64.tar.gz
cd imgsrch-darwin-arm64
./imgsrch setup
```

`setup` downloads the pinned caption, OCR, and embedding models into
`~/.imgsrch/models`. The download is about 3.4 GB and happens once.

## Use

```bash
./imgsrch init my-images
./imgsrch index my-images ~/Pictures/*.png
```

`index` copies the images into `my-images/images` and queues the work.
Supported formats are PNG, JPEG, and GIF. Originals are not modified. Use
`add` to stage images without indexing. Commands refuse a project that does
not exist — `init` creates it; a typo never silently becomes a new project.
The current MVP uses an explicit project directory; direct in-place indexing
of an arbitrary folder is not implemented yet.

Indexing returns after queuing work. The caption and OCR stages finish in the
background, followed by embedding; each model exits when its stage is empty.
Search later, or optionally inspect read-only progress:

```bash
./imgsrch status my-images
./imgsrch search my-images "diagram explaining KV cache"
```

`status` only reports durable progress. It is never required to move indexing
forward.

Run `./imgsrch doctor my-images` when setup or model discovery fails.

## How search works

Indexing writes plain-text artifacts per image; search fuses two rankings
over them.

```text
index:  image ──► caption model ──► caption.txt ─┐
              └─► OCR model ─────► ocr.txt ──────┴─► combined.md ─► embedding.json

search: query ──► embedding ──► cosine against every image ──► dense ranks ─┐
        query ──► BM25 over combined.md ─────────────────────► keyword ranks┴─► RRF
```

RRF (reciprocal rank fusion) scores each image by summing `1/(60 + rank)`
across both lists. It fuses ranks, not raw scores — neither signal can drown
the other, and an image that is good in both lists beats one that is perfect
in one and missing from the other.

The choices, and why:

- **RRF is the default because it measured better** — higher top-1 and top-3
  recall than the original 50/50 dense + normalized-BM25 blend on the local
  hard set. The old blend is kept as `--scorer simple` so the claim stays
  checkable.
- **Captions are capped at 900 characters** in `combined.md` — verbose
  captions dilute the embedding. Proven by A/B, not taste.
- **Embedding prefixes matter** — documents embed as `search_document: …`,
  queries as `search_query: …` (nomic-embed's contract). The wrong prefix
  quietly degrades everything.
- **Ties break on path**, so results are stable across runs.

Change a weight, swap a model, add a signal — then run `eval` below and see
whether it actually got better.

## Evaluate search quality

Create a hard set as JSON. Expected images may be project-relative paths,
basenames, or content keys:

```json
{
  "queries": [
    { "query": "docker error screen", "expected": ["images/docker.png"] },
    { "query": "diagram explaining queues", "expected": ["queue-diagram.png"] }
  ]
}
```

Compare the default RRF scorer against the original simple blend:

```bash
./imgsrch eval my-images hardset.json --top-k 5
./imgsrch search my-images "docker error screen" --scorer simple
```

`rrf` is the default search scorer. Use `--scorer simple` to compare against the
original 50/50 dense + normalized BM25 blend.

## Inspect

The product commands hide the engine terminology, but the state remains plain
files under `my-images/.imgsrch/`: workspaces, prompts, results, artifacts, and
the search index. nrvna remains discoverable without becoming setup burden.

The implementation in this directory is the supported imgsrch product. The
root repository still carries `scripts/nrvna-lib.sh` as a helper for people
using the nrvna primitives directly.
