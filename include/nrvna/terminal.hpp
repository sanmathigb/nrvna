/*
 * nrvna - Terminal output helpers
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdlib>
#include <unistd.h>

namespace nrvna::terminal {

inline bool colorEnabled(bool isTerminal, const char* noColor) noexcept {
    return isTerminal && noColor == nullptr;
}

inline bool stderrIsTerminal() noexcept {
    return ::isatty(STDERR_FILENO) == 1;
}

inline bool stderrColorEnabled() noexcept {
    return colorEnabled(stderrIsTerminal(), std::getenv("NO_COLOR"));
}

inline const char* ansi(bool enabled, const char* sequence) noexcept {
    return enabled ? sequence : "";
}

}  // namespace nrvna::terminal
