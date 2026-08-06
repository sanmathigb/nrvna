/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <exception>
#include <optional>
#include <string>

#include "nlohmann/json.hpp"

namespace nrvna {

// Validate structured output before publication.
// Today only JSON Schema output needs post-generation validation.
inline std::optional<std::string> validateStructuredOutput(const std::string& output,
                                                           const std::string& outputFormat) {
    if (outputFormat != "json_schema") {
        return std::nullopt;
    }

    try {
        (void)nlohmann::json::parse(output);
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::string("Structured job produced invalid JSON: ") + e.what();
    }
}

} // namespace nrvna
