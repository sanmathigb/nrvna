# imgsrch: how to run

`imgsrch` is a small nrvna-powered demo that turns images/screenshots into searchable local artifacts.

It assumes the repo is built and the default models already exist under `./models`.

## 1. Build nrvna

```bash
cmake -S . -B build
cmake --build build -j4
```

## 2. Check setup

```bash
scripts/imgsrch doctor
```

Expected model defaults:

```text
models/LFM2.5 VL 1.6B GGUF.gguf
models/mmproj-LFM2.5-VL-1.6B-Q8_0.gguf
models/GLM-OCR-Q8_0.gguf
models/mmproj-GLM-OCR-Q8_0.gguf
models/nomic-embed-text-v1.5.Q8_0.gguf
```

## 3. Quick demo

Use the checked-in media-search images:

```bash
scripts/imgsrch demo data/test-media-search imgsrch-demo
```

This creates:

```text
imgsrch-demo/
├── images/
├── index.qmd
├── search-results.md
└── .imgsrch/
    ├── items.tsv
    ├── workspaces/
    ├── artifacts/
    └── index/
```

`demo` queues image work, then collects and runs a sample search as results land, and writes `index.qmd`.

A live demo is the one time someone is watching — that's the case to prepare for. Pre-index the project beforehand so search is instant:

```bash
scripts/imgsrch status my-shots
```

or point at an already-indexed project:

```bash
scripts/imgsrch search <pre-indexed-project> "query"
```

`scripts/imgsrch demo` is for unattended runs — submit and walk away.

## 4. Use your own screenshots

```bash
scripts/imgsrch init my-shots
scripts/imgsrch add my-shots ~/Desktop/*.png
scripts/imgsrch index my-shots
```

`index` is async. It starts local nrvna daemons, queues caption/OCR jobs, and returns.

Come back later:

```bash
scripts/imgsrch status my-shots
scripts/imgsrch search my-shots "KV cache diagram"
scripts/imgsrch render my-shots
```

## 5. Stop background workers

When done:

```bash
scripts/imgsrch stop my-shots
```

## What happens under the hood

For each image:

```text
image → caption.txt
image → ocr.txt
caption + OCR → combined.md
combined.md → embedding.json
```

Search runs over the ready artifacts using simple keyword matching + embedding similarity.

You can inspect everything:

```bash
find my-shots/.imgsrch/artifacts -type f
cat my-shots/.imgsrch/artifacts/<image-id>/combined.md
```

The folder is the memory.
