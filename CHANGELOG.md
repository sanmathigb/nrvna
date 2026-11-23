# Changelog

## 0.2.0 (2026-11-23)

- Install matching agent and reference guides alongside the commands. Include
  referenced docs and the optional agent skill in release archives.
- Clarify release syntax differences, batch use, model setup, result validation,
  and application restart behavior in the agent guide.

- Update the llama.cpp submodule to v1.0.0. Use its memory-fit API for automatic
  GPU placement in text, vision, and transcription runs when supported. TTS
  models and vocoders remain CPU-only.
- Add `wrk --id` for caller-addressable, idempotent submissions. Repeated IDs
  return the existing job; callers use a new ID for retries.
- Reference image and audio inputs by path by default. Use `wrk --keep-files`
  to copy and retain them in the job. Add `nrvnad clean` to preview or remove
  copied attachments from terminal jobs.
- Breaking CLI change: remove the redundant `--stt` flag. Use `--audio` to
  submit a transcription job.
- Installed macOS executables search `../lib` for shared backend libraries.
- Breaking CLI change: start a daemon with `nrvnad <workspace> <model>`.
  Lifecycle commands remain unchanged.
- Experimental: support [Liquid AI LFM3-VL-2B GGUF](https://huggingface.co/LiquidAI/LFM3-VL-2B-GGUF)
  on the tested llama.cpp CPU build b11960 (`8642541b2`). Tested Q4_K_M weights
  and a Q8_0 projector. Use temperature 0.1, top-k 50, top-p 1.0, min-p 0,
  and repeat penalty 1.1. Pass `--mmproj` when the projector filename does not
  match the model filename. One full-resolution screenshot task completed, but this does
  not establish accuracy for all images or prompts. It took many minutes on
  CPU. nrvna's job interface does not support video or mixed audio and video
  jobs.
