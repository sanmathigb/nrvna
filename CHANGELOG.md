# Changelog

## Unreleased

- `wrk -a <file>` and `wrk --audio <file>` now submit transcription jobs by
  default. Use `--stt` when you want to state the mode explicitly.
- Installed macOS executables search `../lib` for shared backend libraries.
- Breaking CLI change: start a daemon with `nrvnad <workspace> <model>`.
  The `nrvna_start` shell helper uses the same argument order. Lifecycle
  commands remain unchanged.
- Experimental: support [Liquid AI LFM3-VL-2B GGUF](https://huggingface.co/LiquidAI/LFM3-VL-2B-GGUF)
  on the tested llama.cpp b11960 CPU build. Tested Q4_K_M weights and a Q8_0
  projector. Use temperature 0.1, top-k 50, top-p 1.0, min-p 0, and repeat
  penalty 1.1. Pass `--mmproj` when the projector filename does not match the
  model filename. One full-resolution screenshot task completed, but this does
  not establish accuracy for all images or prompts. It took many minutes on
  CPU. Video and mixed audio/video jobs are not supported by nrvna's job
  interface.
