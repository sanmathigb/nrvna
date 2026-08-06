# Configuration

`nrvnad` reads model and runtime settings from environment variables. A command
flag overrides the related variable. These flags include `--workers`,
`--mmproj`, and `--vocoder`.

Most users need only these:

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_MODELS_DIR` | discovered `models/` | Model search directory |
| `NRVNA_WORKERS` | `4` | Worker threads |
| `NRVNA_GPU_LAYERS` | `0` | Layers offloaded to a supported GPU backend |
| `NRVNA_MAX_CTX` | `8192` | Per-job context limit |
| `NRVNA_PREDICT` | `2048` | Maximum generated tokens |
| `NRVNA_MAX_RECOVERY_ATTEMPTS` | `3` | Orphaned `processing/` recoveries before terminal failure |

CPU inference is the default. `nrvnad` resolves model names under `./models` or
`NRVNA_MODELS_DIR`. It also accepts a full model path. It detects matching
projector and vocoder files beside the model.

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
| `NRVNA_VISION_TEMP` | `0.3` | Vision sampling temperature |
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

## Logs and terminal output

| Variable | Default | Purpose |
| --- | --- | --- |
| `NRVNA_LOG_LEVEL` | `info` | nrvna logging: error, warn, info, debug, trace |
| `LLAMA_LOG_LEVEL` | `error` | llama.cpp logging: error, warn, info, debug |
| `NO_COLOR` | unset | Any value disables ANSI color in terminal diagnostics |

## Repository integrations

Repository helpers and applications use these variables. Applications can set
runtime defaults. They use the same nrvna inference path.

| Variable | Default | Used by |
| --- | --- | --- |
| `NRVNA_BUILD_DIR` | repository `build/` | Shell helper, imgsrch, bckbrnr |
| `NRVNA_DAEMON_BIN` | discovered `nrvnad` | Shell helper and imgsrch |
| `NRVNA_WRK_BIN` | discovered `wrk` | imgsrch |
| `NRVNA_FLW_BIN` | discovered `flw` | imgsrch |
| `NRVNA_START_TIMEOUT` | `120` seconds | Persistent-daemon startup helper |
| `NRVNA_LOG_DIR` | `/tmp` | Shell-helper daemon logs |

### imgsrch

`<project>/.imgsrch/config` accepts the same model keys. Environment variables
override values in that file.

| Variable | Default | Purpose |
| --- | --- | --- |
| `IMGSRCH_MODELS_DIR` | `~/.imgsrch/models` | Managed model directory |
| `CAPTION_MODEL` | managed LFM2.5 VL model | Caption-model path |
| `CAPTION_MMPROJ` | managed LFM2.5 projector | Caption-projector path |
| `OCR_MODEL` | managed GLM-OCR model | OCR-model path |
| `OCR_MMPROJ` | managed GLM-OCR projector | OCR-projector path |
| `EMBED_MODEL` | managed nomic-embed model | Embedding-model path |
| `CAPTION_PROMPT` | built-in caption prompt | Caption instruction |
| `OCR_PROMPT` | `Text Recognition:` | Model-specific OCR instruction |
| `META_DOC_PREFIX` | `search_document: ` | Prefix applied to indexed text |
| `META_QUERY_PREFIX` | `search_query: ` | Prefix applied to queries |

### bckbrnr

| Variable | Default | Purpose |
| --- | --- | --- |
| `BCKBRNR_ENGINE_DIR` | bundled engine directory | Directory containing `nrvnad`, `wrk`, and `flw` |
| `BCKBRNR_TEXT_MODEL` | saved selection | Text-model path for tests and development launches |

bckbrnr sets conservative defaults for `NRVNA_GPU_LAYERS=0`,
`NRVNA_TEMP=0.3`, `NRVNA_PREDICT=1024`, `NRVNA_MAX_CTX=4096`, and
`NRVNA_THINKING=0`. Existing environment values override these defaults.
