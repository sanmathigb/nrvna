# imgsrch

Search local screenshots and images by what they say and what they mean.

`imgsrch` reads each image once with local vision models — a caption of what
it shows, OCR of what it says — then searches those words by meaning and by
keyword. Indexing runs in the background; search is offline. Humans get
ranked originals. Agents get a small, cited candidate set.

## Install

Download the release archive for your platform and fetch the models once:

```bash
tar -xzf imgsrch-darwin-arm64.tar.gz
cd imgsrch-darwin-arm64
./imgsrch setup
```

`setup` downloads the pinned caption, OCR, and embedding models into
`~/.imgsrch/models` — about 3.4 GB, checksum-verified, one time. Everything
else ships in the archive: the `imgsrch` binary and the engine binaries in
`bin/`. Keep them together.

Platforms: macOS 13.3+ (Apple Silicon and Intel) and x86-64 Linux with AVX2
(Ubuntu 22.04 compatible). CPU-first — no GPU required. The macOS archives
are not yet signed or notarized; the first run may need approval under
**System Settings → Privacy & Security**.

## Use

```bash
./imgsrch init shots
./imgsrch index shots ~/Screenshots/*.png
./imgsrch search shots "diagram explaining KV cache"
```

Commands refuse a project that does not exist; `init` creates it.

`index` copies images into `shots/images/` (PNG, JPEG, GIF; originals
untouched) and returns after queuing. Caption and OCR finish in the
background — each model loads, drains its queue, and exits. Close the
terminal; the work continues.

Progress is optional to watch:

```bash
./imgsrch status shots
```

`status` is read-only and never required to move indexing forward. When
setup or model discovery fails, `./imgsrch doctor shots` explains why.

## Find a screenshot

Search with the part you remember:

```bash
./imgsrch search shots "that post about engineering ownership"
```

```text
[0.033] images/IMG_7741.PNG
        Screenshot of a post about engineering ownership — "strong teams
        own outcomes, not tickets" …
[0.031] images/Screenshot 2026-03-14 at 9.12.03.png
        Slide titled "Ownership vs. accountability" with a two-column
        comparison …
```

Results print ranked filenames with the caption and visible text that
matched. Search also writes `shots/search-results.md`, with links to the
originals, for any Markdown viewer.

## Give an agent visual memory

Ask an agent to retrieve first and open only the strongest candidates:

```text
Use ./imgsrch to find screenshots in ./shots relevant to my engineering
management interview. Search locally first. Do not enumerate or open the
whole image directory. Use the returned captions and visible-text snippets
to narrow the results. Open at most three original images when visual
verification is necessary. Cite the project-relative filename for every
source.
```

The retrieval step is an ordinary command:

```bash
./imgsrch search shots "engineering management culture hiring" --top-k 10
```

Measured on a real 70-screenshot collection: one search produced ten
grounded interview leads with zero original images loaded into the agent's
context. A candidate-reduction result, not a universal token-savings claim —
the cost after retrieval depends on how many results the agent inspects.

All indexing and search inference stays local. A cloud agent receives the
terminal output and any originals you explicitly allow it to open.

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
across both lists — it fuses ranks, not incomparable raw score scales.

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

## Evaluate search quality

`eval` is a maintainer tool for deciding whether a ranking or model change
actually improves retrieval on a labeled collection.

Expected images may be project-relative paths, basenames, or content keys:

```json
{
  "queries": [
    { "query": "docker error screen", "expected": ["images/docker.png"] },
    { "query": "diagram explaining queues", "expected": ["queue-diagram.png"] }
  ]
}
```

```bash
./imgsrch eval shots hardset.json --top-k 5
./imgsrch search shots "docker error screen" --scorer dense
./imgsrch search shots "docker error screen" --scorer bm25
```

`rrf` is the default scorer. `--scorer simple` compares against the original
blend; `dense` and `bm25` inspect either signal alone.

## Inspect

The index is plain files under `shots/.imgsrch/`:

```text
shots/.imgsrch/
├── artifacts/<image>/   caption.txt · ocr.txt · combined.md · embedding.json
├── index/index.tsv      the search index — a TSV you can read
└── workspaces/          the engine's job queues, inspectable mid-flight
```

If a search result surprises you, the reason is a file you can open.

The implementation in this directory is the supported imgsrch product. The
root repository carries `scripts/nrvna-lib.sh` for people using the nrvna
primitives directly.

---

Built with [nrvna](../../README.md) — local async inference primitives.
