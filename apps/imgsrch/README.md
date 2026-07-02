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
./imgsrch add my-images ~/Pictures/*.png
./imgsrch index my-images
```

`add` copies supported images into `my-images/images`. Supported formats are
PNG, JPEG, and GIF. Originals are not modified. The current MVP uses an
explicit project directory; direct in-place indexing of an arbitrary folder is
not implemented yet.

Indexing returns after queuing work and continues in local background workers.
Check progress and search:

```bash
./imgsrch status my-images
./imgsrch search my-images "diagram explaining KV cache"
```

Stop the workers when needed:

```bash
./imgsrch stop my-images
```

Run `./imgsrch doctor my-images` when setup or model discovery fails.

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
