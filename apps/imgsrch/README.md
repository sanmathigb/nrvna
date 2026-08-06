# imgsrch

[![CI](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml/badge.svg)](https://github.com/sanmathigb/nrvna/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://github.com/sanmathigb/nrvna/blob/main/LICENSE)
[![Built with nrvna](https://img.shields.io/badge/built_with-nrvna-5b5bd6.svg)](https://github.com/sanmathigb/nrvna)

Search local screenshots and images by what they say and what they mean.

`imgsrch` processes each image once with local models. It describes what the
image shows. It also uses OCR to extract visible text. Search uses both meaning
and keywords.

```text
images  ->  local index  ->  "that diagram about KV cache"  ->  originals
```

Indexing runs in the background. Search returns a small, cited candidate set
for a person or agent to inspect.

**Pre-beta.** imgsrch is the first application built with
[nrvna](https://github.com/sanmathigb/nrvna).

## Why

My iPhone screenshots kept accumulating. I knew that useful images were in the
collection. Giving hundreds of images to an agent was not practical. imgsrch
processes them locally once. It retrieves a small relevant set. A person or
agent opens only the evidence needed for the task.

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

`setup` downloads three models for captioning, OCR, and embedding. The models
use five GGUF files, including two vision projectors. They use about 3.4 GB.
`setup` verifies each checksum. You do not need an account or API key.

The archives support macOS 13.3+ on Apple Silicon and Intel. They also support
x86-64 Linux with AVX2 and Ubuntu 22.04. CPU inference is the default. The
macOS previews are not notarized. The first launch can require approval under
**System Settings → Privacy & Security**.

## Search your images

Create a project, import a folder, and search:

```bash
./imgsrch init shots
./imgsrch index shots "$HOME/Screenshots"/*
./imgsrch search shots "diagram explaining KV cache"
```

The current CLI accepts image files. The shell glob imports files directly
from that folder. It does not search subdirectories.

`index` copies supported images into `shots/images/`. It returns after it
queues them. imgsrch supports PNG, JPEG, and GIF files. It does not change the
originals. Captioning and OCR continue in the background. Each model exits
after it drains its work. Run `index` again when you add images. imgsrch skips
content that it already indexed.

Before copying, imgsrch validates the image header and decoded dimensions. It
skips exact duplicates, even when their file names differ. It keeps different
images that share a file name. The later copy receives a stable hash suffix.
imgsrch rejects images above 64 million decoded pixels before model loading.
This limit prevents excessive memory use. Make a smaller copy for indexing.
Keep the original unchanged.

Initial indexing uses significant compute. A high-resolution phone screenshot
can take several minutes on an older CPU. imgsrch does not process completed
images again.

Search prints ranked paths and the evidence behind each match:

```text
1  images/IMG_7741.PNG
   Screenshot of a post about engineering ownership: "strong teams own
   outcomes, not tickets" …

2  images/Screenshot 2026-03-14 at 9.12.03.png
   Slide titled "Ownership vs. accountability" with a two-column
   comparison …
```

It also writes `shots/search-results.md`. The report contains inline links to
the original images. Search needs project write access. It submits a query
embedding job and updates the report.

## Give it to an agent

For a new installation, use an agent with shell access. Supported examples
include Codex CLI, Claude Code, OpenCode, Pi, Hermes, and OpenClaw. Paste:

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
command output. It sees an original only when you permit access.

## Check progress

```bash
./imgsrch status shots
```

`status` is read-only. Indexing does not depend on it. Run this command when
setup or model discovery fails:

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

imgsrch does not support `--json`. Agents should use the concise terminal
results and the detailed `search-results.md` report. The nrvna commands have a
separate JSON contract. Do not pass their flags to imgsrch.

## How search works

```text
index:  image ──► caption ──► caption.txt ─┐
              └─► OCR ─────► ocr.txt ──────┴─► combined.md ─► embedding.json

search: query ──► embedding ─► dense ranking ─┐
        query ──► BM25 ──────► keyword ranking┴─► RRF ─► results
```

RRF means reciprocal rank fusion. It combines dense and keyword ranks. It does
not mix their different raw scores.

The defaults are measured decisions:

- RRF produced better top-1 and top-3 recall than the original normalized
  50/50 blend on the local hard set.
- Captions are capped at 900 characters because verbose captions diluted the
  indexed text embedding.
- Indexed text uses nomic-embed's `search_document:` prefix; queries use
  `search_query:`.
- Ties break on path, so repeated searches are stable.

These choices are current defaults. They are not universal retrieval claims.

## What to expect

imgsrch works best on screenshots, slides, and diagrams. It also works on
images that visible text or a short caption can describe.

It is less reliable with tiny text, blurry text, or purely visual similarity.
It does not know dates or source applications unless the image shows them. The
CLI imports a non-recursive file list. It has no watch mode. Tests use a small
personal collection. They do not show quality at large public scale.

## Inspect and evaluate

Every indexed image has ordinary artifacts:

```text
shots/.imgsrch/
├── artifacts/<image>/   caption.txt · ocr.txt · combined.md · embedding.json
├── index/index.tsv      readable search index
└── workspaces/          durable nrvna jobs
```

If a result surprises you, inspect its caption, OCR, embedding, and component
ranks.

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

If something breaks, [open an issue](https://github.com/sanmathigb/nrvna/issues).
Include a failed real search when possible. This evidence is more useful than
a request based on an imagined workflow.

MIT licensed. Model licenses remain model-specific.
