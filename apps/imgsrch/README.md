# imgsrch

Search local screenshots and images by what they say and what they mean.

`imgsrch` reads each image once with local models, then searches the resulting
captions and visible text. Indexing runs in the background. Humans get ranked
originals; agents get a small, grounded candidate set instead of an entire
image library.

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

Release archives support macOS 13.3 or newer on Apple Silicon and Intel, and
64-bit AVX2 Linux systems compatible with Ubuntu 22.04. The macOS archives are
not yet signed or notarized; they are developer previews and may require
approval in **System Settings > Privacy & Security** the first time they run.

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

## Demo: find a screenshot

Suppose your screenshots have opaque names such as `IMG_7741.PNG`. Index them
once:

```bash
./imgsrch init screenshots
./imgsrch index screenshots ~/Downloads/Screenshots/*.PNG
```

The command returns after queuing the work. Close the terminal or do something
else. Later, search with the part you remember:

```bash
./imgsrch search screenshots "that post about engineering ownership"
```

The terminal prints ranked filenames with the caption and visible text that
made each result relevant. It also writes `screenshots/search-results.md`, with
links to the copied originals, for preview in a Markdown viewer.

## Demo: give an agent visual memory

A vision-capable agent should not inspect a whole screenshot library. Ask it to
retrieve first and look at only the strongest candidates:

```text
Use ./imgsrch to find screenshots in ./screenshots relevant to my engineering
management interview. Search locally first. Do not enumerate or open the whole
image directory. Use the returned captions and visible-text snippets to narrow
the results. Open at most three original images when visual verification is
necessary. Cite the project-relative filename for every source.
```

The retrieval step is an ordinary command the agent can run:

```bash
./imgsrch search screenshots \
  "engineering management leadership culture hiring execution feedback" \
  --top-k 10
```

In a measured 70-screenshot run, one search produced ten grounded interview
leads while loading zero original images into the agent's context. That is a
candidate-reduction result, not a universal token-savings claim: the cost after
retrieval depends on how many results the agent chooses to inspect.

All indexing and search inference stays local. In this workflow, a cloud agent
receives the terminal output and any original images you explicitly allow it
to open.

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
across both lists. It fuses ranks rather than incomparable raw score scales.
An image that ranks well in both lists is rewarded, while either list can still
influence the final order.

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

Most users never run `eval`. It is a maintainer tool for deciding whether a
ranking or model change actually improves retrieval on a labeled collection.

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

Compare the fused scorers and their independent retrieval signals:

```bash
./imgsrch eval my-images hardset.json --top-k 5
./imgsrch search my-images "docker error screen" --scorer simple
./imgsrch search my-images "docker error screen" --scorer dense
./imgsrch search my-images "docker error screen" --scorer bm25
```

`rrf` is the default search scorer. Use `--scorer simple` to compare against the
original blend, or `dense` and `bm25` to inspect either signal alone.

## Inspect

The product commands hide the engine terminology, but the state remains plain
files under `my-images/.imgsrch/`: workspaces, prompts, results, artifacts, and
the search index. nrvna remains discoverable without becoming setup burden.

The implementation in this directory is the supported imgsrch product. The
root repository still carries `scripts/nrvna-lib.sh` as a helper for people
using the nrvna primitives directly.
