/*
 * nrvna - Shared llama.cpp utilities (internal)
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "llama.h"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace nrvna {

inline void warn_invalid_env(const char* name, const char* value, const char* type, const std::string& fallback) {
    std::fprintf(stderr, "nrvna: invalid %s for %s='%s', using default %s\n", type, name, value, fallback.c_str());
}

// Get integer from env with default. Invalid or partial values fall back.
inline int env_int(const char* name, int defv) {
    if (const char* v = std::getenv(name)) {
        errno = 0;
        char* end = nullptr;
        long parsed = std::strtol(v, &end, 10);
        if (end == v || *end != '\0' || errno == ERANGE ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            warn_invalid_env(name, v, "integer", std::to_string(defv));
            return defv;
        }
        return static_cast<int>(parsed);
    }
    return defv;
}

// Get a positive integer from env. Values <= 0 are invalid for llama batch
// sizes and loop strides; falling back prevents hangs like `NRVNA_BATCH=0`.
inline int env_positive_int(const char* name, int defv) {
    int value = env_int(name, defv);
    if (value <= 0) {
        if (const char* raw = std::getenv(name)) {
            warn_invalid_env(name, raw, "positive integer", std::to_string(defv));
        }
        return defv > 0 ? defv : 1;
    }
    return value;
}

// Get float from env with default. Invalid or partial values fall back.
inline float env_float(const char* name, float defv) {
    if (const char* v = std::getenv(name)) {
        errno = 0;
        char* end = nullptr;
        float parsed = std::strtof(v, &end);
        if (end == v || *end != '\0' || errno == ERANGE || !std::isfinite(parsed)) {
            warn_invalid_env(name, v, "float", std::to_string(defv));
            return defv;
        }
        return parsed;
    }
    return defv;
}

inline int effective_gpu_layers() {
    return env_int("NRVNA_GPU_LAYERS", 0);
}

// Filter llama.cpp logs to keep application output clean.
inline void filtered_llama_log(enum ggml_log_level level, const char* text, void* /*user_data*/) {
    if (!text || text[0] == '.' || text[0] == '\n' || text[0] == '\0') return;

    static int filter_level = -1;
    if (filter_level == -1) {
        const char* env = std::getenv("LLAMA_LOG_LEVEL");
        filter_level = env ?
            (std::string(env) == "info" ? GGML_LOG_LEVEL_INFO :
             std::string(env) == "warn" ? GGML_LOG_LEVEL_WARN :
             std::string(env) == "debug" ? GGML_LOG_LEVEL_DEBUG :
             GGML_LOG_LEVEL_ERROR) : GGML_LOG_LEVEL_ERROR;
    }

    if (level >= filter_level) {
        fprintf(stderr, "%s", text);
    }
}

} // namespace nrvna
