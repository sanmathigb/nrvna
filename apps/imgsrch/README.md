# imgsrch

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/sanmathigb/nrvna/blob/main/LICENSE)
[![Built with nrvna](https://img.shields.io/badge/built_with-nrvna-5b5bd6.svg)](https://github.com/sanmathigb/nrvna)

Search local screenshots and images by what they say and what they mean.

`imgsrch` processes each image once with local models. It generates a caption
of what the image shows and OCR of what it says, then searches both by meaning
and keyword.

```text
images  ->  local index  ->  "that diagram about KV cache"  ->  originals
```

Indexing runs in the background. Search returns a small, cited candidate set
for a person or agent to inspect.

**Pre-beta.** imgsrch is the first application built with
[nrvna](https://github.com/sanmathigb/nrvna).

## Install

Download the archive for your platform from the
[latest release](https://github.com/sanmathigb/nrvna/releases/latest):

| Platform | Archive |
| --- | --- |
| Apple Silicon | `imgsrch-darwin-arm64.tar.gz` |
| Intel Mac | `imgsrch-darwin-x86_64.tar.gz` |
| x86-64 Linux | `imgsrch-linux-x86_64.tar.gz` |

Extract it, keep `imgsrch` beside `bin/`, and fetch the models once:

```bash
tar -xzf imgsrch-darwin-arm64.tar.gz
cd imgsrch-darwin-arm64
./imgsrch setup
./imgsrch doctor
```

`setup` downloads three logical models for captioning, OCR, and embedding.
They occupy about 3.4 GB as five checksum-verified GGUF files, including two
vision projectors. No account or API key is required.

The archives support macOS 13.3+ on Apple Silicon and Intel, and x86-64 Linux
with AVX2 (Ubuntu 22.04 compatible). CPU inference is the default. The macOS
developer previews are not notarized; first launch may require approval under
**System Settings → Privacy & Security**.

## Search your images

Create a project, import a folder, and search:

```bash
./imgsrch init shots
./imgsrch index shots "$HOME/Screenshots"/*
./imgsrch search shots "diagram explaining KV cache"
```

The current CLI accepts image files, so the shell glob imports files directly
inside that folder; it does not recurse into subdirectories.

`index` copies supported images into `shots/images/` and returns after queuing
them. PNG, JPEG, and GIF are supported. Originals are left untouched.
Captioning and OCR continue in the background, and each model exits after
draining its work. Re-run `index` as images accumulate; already indexed
content is skipped.

Before copying, imgsrch validates each image header and decoded dimensions.
Exact content duplicates are skipped even when their filenames differ. If two
different images share a filename, both are retained and the later copy gets a
deterministic hash suffix. Images above 64 million decoded pixels are rejected
before model loading to avoid unbounded memory expansion; make a smaller copy
for indexing while keeping the original untouched.

Initial indexing is compute-heavy. A high-resolution phone screenshot may
take several minutes on an older CPU; completed images do not need to be
processed again.

Search prints ranked paths and the evidence behind each match:

```text
1  images/IMG_7741.PNG
   Screenshot of a post about engineering ownership: "strong teams own
   outcomes, not tickets" …

2  images/Screenshot 2026-03-14 at 9.12.03.png
   Slide titled "Ownership vs. accountability" with a two-column
   comparison …
```

It also writes `shots/search-results.md` with inline, clickable previews of
the original images. Search requires write access to the project because it
submits a query-embedding job and updates this report.

## Give it to an agent

For a new installation, paste this into Codex CLI, Claude Code, OpenCode, Pi,
Hermes, OpenClaw, or another agent with shell access:

```text
Set up imgsrch for me by following
https://raw.githubusercontent.com/sanmathigb/nrvna/main/apps/imgsrch/INSTALL.md
exactly. Run the steps yourself, verify each step, and ask me only where the
guide says to.
```

Once a project exists, ask the agent to retrieve before opening originals:

```text
Use ./imgsrch to find the strongest screenshots in ./shots for my engineering
management interview. Do not enumerate or open the whole image directory.
Search with the terms you need, use the returned captions and visible-text
snippets to narrow the results, and open at most three originals for visual
verification. Then prepare a concise interview brief and cite the
project-relative filename for every source.
```

All indexing and search inference stays on this machine. A remote agent sees
only command output and any originals you explicitly allow it to open.

## Check progress

```bash
./imgsrch status shots
```

`status` is read-only; it is not required to advance indexing. If setup or
model discovery fails, run:

```bash
./imgsrch doctor shots
```

## Which command to use

| Command | When to use it |
| --- | --- |
| `setup` | Once per model cache, during installation or after a model update. |
| `doctor [project]` | After setup, or when model or engine discovery fails. |
| `init <project>` | Once for a new collection. Reusing an existing project is safe. |
| `index <project> [images...]` | Normal ingestion command. Add new images and start or resume background indexing. |
| `status <project>` | Read progress once when returning. It never advances indexing. |
| `search <project> <query>` | Normal retrieval command after at least one image is indexed. RRF is the default. |
| `add <project> <images...>` | Stage files without starting indexing. Most people and agents should use `index` instead. |
| `eval <project> <hardset.json>` | Maintainer workflow for labeled retrieval experiments, not ordinary search. |

imgsrch does not currently expose `--json`. Agents should consume the concise
terminal results and the richer `search-results.md` report. The underlying
nrvna commands have their own JSON contract; do not pass those flags through
to imgsrch.

## How search works

```text
index:  image ──► caption ──► caption.txt ─┐
              └─► OCR ─────► ocr.txt ──────┴─► combined.md ─► embedding.json

search: query ──► embedding ─► dense ranking ─┐
        query ──► BM25 ──────► keyword ranking┴─► RRF ─► results
```

RRF (reciprocal rank fusion) combines the dense and keyword ranks without
mixing their incomparable raw scores.

The defaults are measured decisions:

- RRF produced better top-1 and top-3 recall than the original normalized
  50/50 blend on the local hard set.
- Captions are capped at 900 characters because verbose captions diluted the
  image embedding.
- Indexed text uses nomic-embed's `search_document:` prefix; queries use
  `search_query:`.
- Ties break on path, so repeated searches are stable.

These choices are current defaults, not universal retrieval claims.

## What to expect

imgsrch works best on screenshots, slides, diagrams, and images whose meaning
can be recovered from visible text or a short caption.

It is less reliable for tiny or blurry text, purely visual similarity, and
queries that require metadata such as dates or source applications. The
current CLI imports a non-recursive list of files and has no watch mode.
Retrieval quality has been tested on a small personal collection, not yet at
large public scale.

## Inspect and evaluate

Every indexed image has ordinary artifacts:

```text
shots/.imgsrch/
├── artifacts/<image>/   caption.txt · ocr.txt · combined.md · embedding.json
├── index/index.tsv      readable search index
└── workspaces/          durable nrvna jobs
```

If a result surprises you, its caption, OCR, embedding, and component ranks
are inspectable.

Maintainers can compare scorers against a labeled set:

```bash
./imgsrch eval shots hardset.json --top-k 5
./imgsrch search shots "docker error screen" --scorer dense
./imgsrch search shots "docker error screen" --scorer bm25
```

`rrf` is the default. `simple` retains the original blend; `dense` and `bm25`
isolate either retrieval signal.

Model, prompt, and engine-path environment overrides are listed in the
[nrvna configuration reference](https://github.com/sanmathigb/nrvna/blob/main/CONFIGURATION.md).

If something breaks or a real search fails, please
[open an issue](https://github.com/sanmathigb/nrvna/issues). Those examples
are more useful than feature requests based only on imagined workflows.

MIT licensed. Model licenses remain model-specific.
