# Metal discrete-GPU patch (optional, dev-only)

These two patches fix `newBufferWithBytesNoCopy` failing on **Intel Macs with a
discrete GPU** (e.g. Radeon Pro 560) that have non-unified memory and
non-page-aligned tensor pointers. Upstream commit `6fdddb498` ("metal: support
virtual devices") changed discrete GPUs from shared to private buffers and broke
this path.

**You almost certainly do not need these.**

- **Apple Silicon** — unified memory; the broken path is never taken, and these
  patches would replace zero-copy buffers with copies (a small pessimization).
  Build vanilla.
- **Linux** — no Metal backend compiled. Irrelevant.
- **Intel Mac, CPU mode** (the default, `NRVNA_GPU_LAYERS=0`) — GPU buffer code
  is never exercised. Build vanilla.
- **Intel Mac + discrete GPU + opt-in GPU offload** (`NRVNA_GPU_LAYERS>0`) — the
  one case these help.

The shipped product and CI build vanilla upstream llama.cpp. Apply these only on
a dev box that fits the last bullet:

```bash
cd third_party/llama.cpp
git apply ../../patches/*.patch
cd ../.. && cmake --build build
```
