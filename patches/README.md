# Metal discrete-GPU patch (optional, dev-only)

These two patches fix a `newBufferWithBytesNoCopy` failure on some Intel Macs.
The affected Macs use a discrete GPU, non-unified memory, and unaligned tensor
pointers. Upstream commit `6fdddb498` changed discrete GPUs from shared buffers
to private buffers. That change broke this path.

**You almost certainly do not need these.**

- **Apple Silicon:** Do not apply the patches. Apple Silicon uses unified
  memory. The patches would replace zero-copy buffers with copies.
- **Linux:** Do not apply the patches. Linux does not build the Metal backend.
- **Intel Mac in CPU mode:** Do not apply the patches. CPU mode does not use
  the GPU buffer code.
- **Intel Mac with discrete GPU offload:** Apply the patches when
  `NRVNA_GPU_LAYERS>0` causes this failure.

The release and CI use unmodified upstream llama.cpp. Apply these patches only
on an affected development machine:

```bash
cd third_party/llama.cpp
git apply ../../patches/*.patch
cd ../.. && cmake --build build
```
