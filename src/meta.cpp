/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/meta.hpp"
#include "nrvna/contract.hpp"
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

namespace nrvna {

std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (uc < 0x20) {
                    out += "\\u00";
                    out += "0123456789ABCDEF"[((uc >> 4) & 0xF)];
                    out += "0123456789ABCDEF"[uc & 0xF];
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string formatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;

    struct tm tm_buf;
    gmtime_r(&time, &tm_buf);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);

    char result[64];
    std::snprintf(result, sizeof(result), "%s.%06ldZ", buf, static_cast<long>(us));
    return result;
}

bool writeMetaJson(const std::filesystem::path& dir, const JobMeta& meta) {
    try {
        nlohmann::json document;
        document["submitted_at"] = meta.submitted_at;
        document["mode"] = meta.mode;

        if (!meta.parent.empty()) {
            document["parent"] = meta.parent;
        }

        if (!meta.tags.empty()) {
            document["tags"] = meta.tags;
        }

        if (!meta.output_format.empty()) {
            document["output_format"] = meta.output_format;
        }

        if (meta.recovery_attempts > 0) {
            document["recovery_attempts"] = meta.recovery_attempts;
        }

        if (!meta.status.empty()) {
            document["completed_at"] = meta.completed_at;
            document["duration_s"] = std::round(meta.duration_s * 100.0) / 100.0;
            document["artifacts"] = meta.artifacts;
            document["status"] = meta.status;
        }

        auto tmpPath = dir / (std::string(contract::kMetaFile) + ".tmp");
        auto finalPath = dir / contract::kMetaFile;

        {
            std::ofstream file(tmpPath, std::ios::binary);
            if (!file) return false;
            file << document.dump(2) << '\n';
            file.flush();
            if (!file.good()) return false;
        }

        std::filesystem::rename(tmpPath, finalPath);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<JobMeta> readMetaJson(const std::filesystem::path& dir) {
    try {
        auto metaPath = dir / contract::kMetaFile;
        if (!std::filesystem::exists(metaPath)) return std::nullopt;

        std::ifstream file(metaPath, std::ios::binary);
        if (!file) return std::nullopt;

        auto document = nlohmann::json::parse(file);
        if (!document.is_object()) return std::nullopt;
        if (!document.contains("submitted_at") ||
            !document["submitted_at"].is_string() ||
            !document.contains("mode") || !document["mode"].is_string()) {
            return std::nullopt;
        }

        JobMeta meta;
        meta.submitted_at = document["submitted_at"].get<std::string>();
        meta.mode = document["mode"].get<std::string>();
        if (meta.submitted_at.empty() || meta.mode.empty()) return std::nullopt;

        auto readString = [&document](const char* key, std::string& value) {
            if (!document.contains(key)) return true;
            if (!document[key].is_string()) return false;
            value = document[key].get<std::string>();
            return true;
        };
        auto readStrings = [&document](const char* key,
                                       std::vector<std::string>& values) {
            if (!document.contains(key)) return true;
            if (!document[key].is_array()) return false;
            for (const auto& value : document[key]) {
                if (!value.is_string()) return false;
                values.push_back(value.get<std::string>());
            }
            return true;
        };

        if (!readString("parent", meta.parent) ||
            !readStrings("tags", meta.tags) ||
            !readString("output_format", meta.output_format) ||
            !readString("completed_at", meta.completed_at) ||
            !readStrings("artifacts", meta.artifacts) ||
            !readString("status", meta.status)) {
            return std::nullopt;
        }

        if (document.contains("recovery_attempts")) {
            const auto& value = document["recovery_attempts"];
            if (!value.is_number_unsigned()) return std::nullopt;
            auto attempts = value.get<unsigned long long>();
            if (attempts > std::numeric_limits<unsigned int>::max()) {
                return std::nullopt;
            }
            meta.recovery_attempts = static_cast<unsigned int>(attempts);
        }

        if (document.contains("duration_s")) {
            if (!document["duration_s"].is_number()) return std::nullopt;
            meta.duration_s = document["duration_s"].get<double>();
        }

        return meta;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace nrvna
