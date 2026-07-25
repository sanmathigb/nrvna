/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 *
 * The on-disk job contract. Single owner for every path and filename a
 * workspace contains. Rule: if a string names a file or directory inside
 * a workspace, it lives here or it's a bug.
 */
#pragma once
#include "nrvna/types.hpp"
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace nrvna::contract {

// ── State directories, relative to the workspace root ──────────────────
inline constexpr const char* kWritingDir    = "input/writing";  // staging, invisible
inline constexpr const char* kReadyDir      = "input/ready";    // Status::Queued
inline constexpr const char* kProcessingDir = "processing";     // Status::Running
inline constexpr const char* kOutputDir     = "output";         // Status::Done
inline constexpr const char* kFailedDir     = "failed";         // Status::Failed

// ── Files and directories inside a job directory ───────────────────────
inline constexpr const char* kPromptFile     = "prompt.txt";
inline constexpr const char* kTypeFile       = "type.txt";
inline constexpr const char* kMetaFile       = "meta.json";
inline constexpr const char* kResultFile     = "result.txt";
inline constexpr const char* kTranscriptFile = "transcript.txt";
inline constexpr const char* kAudioFile      = "audio.wav";
inline constexpr const char* kEmbeddingFile  = "embedding.json";
inline constexpr const char* kErrorFile      = "error.txt";
inline constexpr const char* kImagesDir      = "images";
inline constexpr const char* kAudioInputDir  = "audio";

// Directory a job in state `s` lives under. Missing has no directory.
inline std::filesystem::path stateDir(const std::filesystem::path& ws, Status s) {
    switch (s) {
        case Status::Queued:  return ws / kReadyDir;
        case Status::Running: return ws / kProcessingDir;
        case Status::Done:    return ws / kOutputDir;
        case Status::Failed:  return ws / kFailedDir;
        default:              return {};
    }
}

inline std::filesystem::path jobDir(const std::filesystem::path& ws, Status s, const JobId& id) {
    auto dir = stateDir(ws, s);
    return dir.empty() ? dir : dir / id;
}

// ── Status names, as written to meta.json and emitted by --json ────────
inline const char* toString(Status s) {
    switch (s) {
        case Status::Queued:  return "queued";
        case Status::Running: return "running";
        case Status::Done:    return "done";
        case Status::Failed:  return "failed";
        default:              return "missing";
    }
}

// ── JobType, as serialized to type.txt ─────────────────────────────────
inline const char* toString(JobType t) {
    switch (t) {
        case JobType::Embed:  return "embed";
        case JobType::Vision: return "vision";
        case JobType::Tts:    return "tts";
        case JobType::Stt:    return "stt";
        default:              return "text";
    }
}

// Unknown or empty spellings mean text — a job with no type.txt is a text job.
inline JobType parseJobType(const std::string& s) {
    if (s == "embed")  return JobType::Embed;
    if (s == "vision") return JobType::Vision;
    if (s == "tts")    return JobType::Tts;
    if (s == "stt")    return JobType::Stt;
    return JobType::Text;
}

// Unlike parseJobType(), this distinguishes an absent type.txt (handled by the
// caller as text) from a present-but-corrupt spelling.
inline std::optional<JobType> tryParseJobType(const std::string& s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return std::nullopt;
    auto last = s.find_last_not_of(" \t\r\n");
    auto type = s.substr(first, last - first + 1);
    if (type == "text")   return JobType::Text;
    if (type == "embed")  return JobType::Embed;
    if (type == "vision") return JobType::Vision;
    if (type == "tts")    return JobType::Tts;
    if (type == "stt")    return JobType::Stt;
    return std::nullopt;
}

// ── Job ID grammar ──────────────────────────────────────────────────────
// Digits and single underscores; no leading/trailing/doubled underscore;
// max 128 chars. (Timestamp_pid_counter, see Work::generateId.)
inline bool isValidJobId(const JobId& id) {
    if (id.empty() || id.size() > 128) return false;
    if (id.front() == '_' || id.back() == '_') return false;
    bool prev_underscore = false;
    for (char c : id) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '_') return false;
        if (c == '_') {
            if (prev_underscore) return false;
            prev_underscore = true;
        } else {
            prev_underscore = false;
        }
    }
    return true;
}

// ── The output artifact rule ────────────────────────────────────────────
// A Done job has exactly one primary artifact; when several exist, this
// priority order decides. Stated once, here.
enum class ArtifactKind : uint8_t { Result, Transcript, Audio, Embedding };

struct Artifact {
    ArtifactKind kind;
    std::filesystem::path path;
};

inline const char* toString(ArtifactKind k) {
    switch (k) {
        case ArtifactKind::Transcript: return "transcript";
        case ArtifactKind::Audio:      return "audio";
        case ArtifactKind::Embedding:  return "embedding";
        default:                       return "result";
    }
}

inline std::optional<Artifact> findOutputArtifact(const std::filesystem::path& jobDir) {
    const std::pair<ArtifactKind, const char*> priority[] = {
        {ArtifactKind::Result,     kResultFile},
        {ArtifactKind::Transcript, kTranscriptFile},
        {ArtifactKind::Audio,      kAudioFile},
        {ArtifactKind::Embedding,  kEmbeddingFile},
    };
    for (const auto& [kind, name] : priority) {
        auto p = jobDir / name;
        std::error_code ec;
        if (std::filesystem::exists(p, ec) && !ec) return Artifact{kind, p};
    }
    return std::nullopt;
}

} // namespace nrvna::contract
