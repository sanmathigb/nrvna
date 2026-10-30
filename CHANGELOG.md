# Changelog

## Unreleased

- Breaking CLI change: start a daemon with `nrvnad <workspace> <model>`.
  The `nrvna_start` shell helper uses the same argument order. Lifecycle
  commands remain unchanged.
- Experimental: add GGUF support for [Liquid AI LFM3-VL-2B](https://huggingface.co/LiquidAI/LFM3-VL-2B-GGUF).
  Smoke-tested text generation and one image question on CPU macOS with
  Q4_K_M weights and a Q8_0 projector. Video and mixed audio/video jobs are
  not supported by nrvna's job interface.
