# Configuration

`nrvnad` reads model and runtime settings from environment variables. A command
flag overrides the related variable. These flags include `--workers`,
`--mmproj`, and `--vocoder`.

Set runtime overrides before starting `nrvnad`. The daemon reads them when it
loads the model.

Most users need only these:

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_MODELS_DIR` | discovered `models/` | Model search directory |
| `NRVNA_WORKERS` | `1` | Concurrent jobs |
| `NRVNA_GPU_LAYERS` | automatic fit | Set `0` for CPU or a positive layer count to override GPU placement |
| `NRVNA_REPACK` | `1` | Set `0` to disable CPU weight repacking and reduce extra memory use |
| `NRVNA_MAX_CTX` | `8192` | Per-job context limit |
| `NRVNA_PREDICT` | `2048` | Maximum generated tokens |
| `NRVNA_MAX_RECOVERY_ATTEMPTS` | `3` | Orphaned `processing/` recoveries before terminal failure |

When the build has a supported GPU backend, llama.cpp estimates GPU placement
from available device memory. It includes the model, worker contexts, and the
projector size when one is set. It leaves llama.cpp's default memory margin.
The estimate does not predict speed. It assumes host memory is available. If
the fit fails, nrvna loads the model on the CPU. Set `NRVNA_GPU_LAYERS=0` to
force CPU placement, or set a positive value to choose a fixed layer count.

`nrvnad` resolves model names under `./models` or `NRVNA_MODELS_DIR`. It also
accepts a full model path. It detects matching projector and vocoder files
beside the model. TTS models and vocoders use CPU placement in this release.

There is no worker count that is fastest on every machine. The default is one
to avoid CPU oversubscription and excess context memory. More workers can run
more jobs at once, but each active job needs its own context and compute
memory. Text and text-embedding contexts divide CPU threads across workers.
Some modality work is serialized: image and audio media evaluation, and LFM3
speech generation. More workers may add memory use without speeding up those
stages. Increase the count only when your workload finishes sooner without
causing memory pressure. Counts above the logical CPU thread count can still
oversubscribe the CPU because each worker gets at least one thread.

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
| `NRVNA_TTS_TEMP` | `1.0` | LFM3 speech-generation temperature |
| `NRVNA_TTS_PREDICT` | `4096` | Maximum LFM3 speech-generation frames |
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
