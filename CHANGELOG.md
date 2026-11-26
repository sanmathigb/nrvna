# Changelog

## 0.2.1 (2026-11-25)

Documentation-only release. The binaries are unchanged from 0.2.0. This release
rebuilds the archives so the bundled guides match the shipped commands.

- Remove the `--stt` flag from the README work-type table. The 0.2.0 binary
  already rejects `--stt`; use `--audio` for transcription.
- Stop suggesting a specific model. The guides tell the caller to bring a
  compatible GGUF, and examples use `./models/your-model.gguf`.
- Show the required `--mmproj` projector in the README vision example and list
  its prerequisites.
- Split the agent chaining example at retrieval, so the validation step comes
  before the next submission.
- State nrvna's limits directly: no token streaming, no response-time
  guarantee, no always-on endpoint, and no transactions. The caller owns those.
- Correct wording: a job is a directory, not a file. List `why.md` in the agent
  index. Remove the stale landing-page draft and unreferenced demo files.
- Known limitation: runtime coverage is still limited across models,
  modalities, and hardware. Validate speech, vision, TTS, GPU, and Linux paths
  on your own hardware.

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
