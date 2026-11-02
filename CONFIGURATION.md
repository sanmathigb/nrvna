# Configuration

`nrvnad` reads model and runtime settings from environment variables. A command
flag overrides the related variable. These flags include `--workers`,
`--mmproj`, and `--vocoder`.

Most users need only these:

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_MODELS_DIR` | discovered `models/` | Model search directory |
| `NRVNA_WORKERS` | `4` | Concurrent jobs |
| `NRVNA_GPU_LAYERS` | `0` | Layers offloaded to a supported GPU backend |
| `NRVNA_REPACK` | `1` | Set `0` to disable CPU weight repacking and reduce extra memory use |
| `NRVNA_MAX_CTX` | `8192` | Per-job context limit |
| `NRVNA_PREDICT` | `2048` | Maximum generated tokens |
| `NRVNA_MAX_RECOVERY_ATTEMPTS` | `3` | Orphaned `processing/` recoveries before terminal failure |

CPU inference is the default. `nrvnad` resolves model names under `./models` or
`NRVNA_MODELS_DIR`. It also accepts a full model path. It detects matching
projector and vocoder files beside the model.

There is no worker count that is fastest on every machine. More workers can run
more jobs at once, but each active job needs its own context and compute
memory. On a memory-limited machine, start with `--workers 1`. Increase the
count only when your workload finishes sooner without causing memory pressure.

`NRVNA_REPACK=0` disables llama.cpp's extra CPU buffer types for CPU model
loads. It can reduce memory use. It can also change speed. Measure both modes
for the model and machine. This setting does not affect GPU model loads.

## Generation

GGUF metadata supplies sampling values when available. Otherwise, nrvna uses
these fallback values.

| Variable | Fallback | Purpose |
| --- | --- | --- |
| `NRVNA_TEMP` | `0.8` | Sampling temperature |
| `NRVNA_TOP_K` | `40` | Top-K sampling |
| `NRVNA_TOP_P` | `0.9` | Top-P sampling |
| `NRVNA_MIN_P` | `0.05` | Min-P sampling |
| `NRVNA_REPEAT_PENALTY` | `1.1` | Repetition penalty |
| `NRVNA_REPEAT_LAST_N` | `64` | Repeat-penalty window |
| `NRVNA_SEED` | `0` | Sampler seed |
| `NRVNA_THINKING` | enabled | Set exactly `0` to disable supported thinking templates |
| `NRVNA_CHAT_TEMPLATE_FILE` | GGUF template | Chat-template override; unreadable files fail startup |

`NRVNA_N_PREDICT` is a legacy alias used only when `NRVNA_PREDICT` is unset.

## Context and batching

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_MAX_CTX` | `8192` | Per-job context limit, capped by the model's trained context |
| `NRVNA_PREDICT` | `2048` | Maximum generated tokens; TTS defaults to `4096` |
| `NRVNA_BATCH` | `2048` | Logical prompt batch size; TTS defaults to `8192` |
| `NRVNA_UBATCH` | batch size | Physical batch size; lower it to reduce peak memory |

Every job receives a new context. These values do not change that rule.
Increasing `NRVNA_MAX_CTX` does not carry state between `wrk` submissions.

## Vision, speech, and media

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_VISION_TEMP` | Model or general temperature; else `0.3` | Vision sampling temperature override |
| `NRVNA_IMAGE_MAX_TOKENS` | model default | Image-token cap; `0` keeps the model default |
| `NRVNA_STT_TEMP` | base temperature | Speech-to-text sampling temperature |
| `NRVNA_STT_PREDICT` | base prediction limit | Speech-to-text generated-token limit |
| `NRVNA_WARMUP` | `0` | Set `1` to warm the multimodal context |
| `NRVNA_FLASH_ATTN` | `-1` | Multimodal flash-attention type; `-1` is automatic |
| `NRVNA_TTS_REPEAT_PENALTY` | `0` | TTS repetition penalty; `0` disables it |
| `NRVNA_TTS_REPEAT_LAST_N` | `128` | TTS repeat window when its penalty is enabled |
| `NRVNA_TTS_MUTE_MS` | `250` | Silence applied at audio start; `0` disables it |
| `NRVNA_MAX_IMAGE_SIZE` | `52428800` | Maximum submitted image size in bytes (50 MiB) |
| `NRVNA_MAX_AUDIO_SIZE` | `209715200` | Maximum submitted audio size in bytes (200 MiB) |
| `NRVNA_MAX_PROMPT_SIZE` | `10000000` | Maximum `prompt.txt` size read by the daemon |

For vision jobs, `NRVNA_VISION_TEMP` has the highest priority. Otherwise,
nrvna uses `NRVNA_TEMP`, then the GGUF temperature hint. It uses `0.3` only
when neither value exists. Other sampling values use their normal precedence.

## Logs and terminal output

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_LOG_LEVEL` | `info` | nrvna logging: error, warn, info, debug, trace |
| `LLAMA_LOG_LEVEL` | `error` | llama.cpp logging: error, warn, info, debug |
| `NO_COLOR` | unset | Any value disables ANSI color in terminal diagnostics |
