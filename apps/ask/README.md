# ask

`ask` retrieves evidence, then submits one answer job to nrvna.

- `ask text` searches an indexed qmd collection.
- `ask images` searches an imgsrch project.
- nrvna generates the answer in a fresh job context.
- The answer and its inputs remain in the ask workspace.

```bash
./apps/ask/ask text "What makes decode slower than prefill?" --collection llama-notes
./apps/ask/ask images ./shots "Which screenshot explains KV cache?"
```

Text mode needs `qmd` and an indexed collection. Image mode needs `imgsrch`
and an indexed project. Set `IMGSRCH_BIN` if `imgsrch` is not on `PATH`.
Build it from this repository with `(cd apps/imgsrch && go build -o ../../build/imgsrch .)`.

The local `Qwen 3.5 4B Q4 K S.gguf` model is the default. Set
`NRVNA_ASK_MODEL` to select another GGUF. nrvna commands default to `./build`.
Set `NRVNA_WRK`, `NRVNA_NRVNAD`, or `NRVNA_FLW` to use other binaries.

The default workspace is `~/.nrvna/ask`. Set `--workspace` to change it.
The command runs `nrvnad --drain`, so no daemon stays open.
