/*
 * nrvna ai - Asynchronous Inference Primitive
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/scanner.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/flow.hpp"
#include "nrvna/logger.hpp"
#include "llama_util.hpp"
#include <algorithm>
#include <fstream>

namespace nrvnaai {

Scanner::Scanner(const std::filesystem::path& workspace) noexcept 
    : workspace_(workspace), readyPath_(workspace / contract::kReadyDir) {
}

std::vector<JobId> Scanner::scan() const noexcept {
    std::vector<JobId> jobs;
    
    try {
        if (!std::filesystem::exists(readyPath_)) {
            LOG_DEBUG("Ready directory does not exist: " + readyPath_.string());
            return jobs;
        }

        for (const auto& entry : std::filesystem::directory_iterator(readyPath_)) {
            if (entry.is_directory()) {
                JobId jobId = extractJobId(entry.path());
                if (Flow::isValidJobId(jobId) && isValidJobDirectory(entry.path())) {
                    jobs.push_back(jobId);
                    LOG_TRACE("Found job: " + jobId);
                }
            }
        }

        // Sort by job ID for consistent ordering (timestamp is in the ID)
        std::sort(jobs.begin(), jobs.end());
        
        if (!jobs.empty()) {
            LOG_DEBUG("Scanner found " + std::to_string(jobs.size()) + " ready jobs");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Scanner error: " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown scanner error");
    }
    
    return jobs;
}

bool Scanner::hasNewJobs() const noexcept {
    try {
        if (!std::filesystem::exists(readyPath_)) {
            return false;
        }

        for (const auto& entry : std::filesystem::directory_iterator(readyPath_)) {
            if (entry.is_directory() &&
                Flow::isValidJobId(extractJobId(entry.path())) &&
                isValidJobDirectory(entry.path())) {
                return true;
            }
        }
    } catch (...) {
        // Swallow errors in this quick check
    }
    
    return false;
}

std::size_t Scanner::readyJobCount() const noexcept {
    std::size_t count = 0;
    
    try {
        if (!std::filesystem::exists(readyPath_)) {
            return 0;
        }

        for (const auto& entry : std::filesystem::directory_iterator(readyPath_)) {
            if (entry.is_directory() &&
                Flow::isValidJobId(extractJobId(entry.path())) &&
                isValidJobDirectory(entry.path())) {
                ++count;
            }
        }
    } catch (...) {
        // Return what we counted so far
    }
    
    return count;
}

bool Scanner::isValidJobDirectory(const std::filesystem::path& dir) const noexcept {
    try {
        // Must be a directory
        if (!std::filesystem::is_directory(dir)) {
            return false;
        }

        // Must contain a bounded regular prompt.txt. The queue is disk-backed,
        // so reject symlinks/FIFOs/huge files before a worker opens them.
        auto promptFile = dir / contract::kPromptFile;
        std::error_code ec;
        auto st = std::filesystem::symlink_status(promptFile, ec);
        if (ec || !std::filesystem::exists(st) || std::filesystem::is_symlink(st) || !std::filesystem::is_regular_file(st)) {
            LOG_DEBUG("Invalid job directory (bad prompt.txt): " + dir.string());
            return false;
        }
        auto promptBytes = std::filesystem::file_size(promptFile, ec);
        if (ec) {
            LOG_DEBUG("Invalid job directory (cannot stat prompt.txt): " + dir.string());
            return false;
        }
        const auto maxPromptBytes = static_cast<std::uintmax_t>(env_positive_int("NRVNA_MAX_PROMPT_SIZE", 10'000'000));
        if (promptBytes > maxPromptBytes) {
            LOG_DEBUG("Invalid job directory (prompt.txt too large): " + dir.string());
            return false;
        }

        // Prompt file must not be empty, except for media-backed jobs that have
        // a useful default prompt at execution time.
        if (promptBytes == 0) {
            auto typeFile = dir / contract::kTypeFile;
            auto imagesDir = dir / contract::kImagesDir;
            auto audioDir = dir / contract::kAudioInputDir;
            std::string type;
            if (std::filesystem::exists(typeFile) && std::filesystem::is_regular_file(typeFile)) {
                std::ifstream in(typeFile);
                std::getline(in, type);
            }
            const JobType jobType = contract::parseJobType(type);
            const bool allowEmptyPrompt =
                (jobType == JobType::Embed && std::filesystem::exists(imagesDir) && std::filesystem::is_directory(imagesDir)) ||
                (jobType == JobType::Stt && std::filesystem::exists(audioDir) && std::filesystem::is_directory(audioDir) && !std::filesystem::is_empty(audioDir));
            if (!allowEmptyPrompt) {
                LOG_DEBUG("Invalid job directory (empty prompt.txt): " + dir.string());
                return false;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

JobId Scanner::extractJobId(const std::filesystem::path& dir) const noexcept {
    try {
        return dir.filename().string();
    } catch (...) {
        return "";
    }
}

}
