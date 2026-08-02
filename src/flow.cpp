/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/flow.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/scanner.hpp"
#include "nrvna/logger.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>

namespace nrvna {

// Convert filesystem time to system time with minimal race window
static std::chrono::system_clock::time_point toSystemTime(
    const std::filesystem::file_time_type& file_time) noexcept {
    const auto file_now = std::filesystem::file_time_type::clock::now();
    const auto sys_now = std::chrono::system_clock::now();
    const auto delta = file_time - file_now;
    return sys_now + std::chrono::duration_cast<std::chrono::system_clock::duration>(delta);
}

Flow::Flow(const std::filesystem::path& workspace) noexcept
    : workspace_(workspace) {
}

std::optional<Job> Flow::latest() const noexcept {
    try {
        std::optional<Job> newest;
        const std::pair<std::filesystem::path, Status> dirs[] = {
            {contract::stateDir(workspace_, Status::Done), Status::Done},
            {contract::stateDir(workspace_, Status::Failed), Status::Failed},
            {contract::stateDir(workspace_, Status::Running), Status::Running},
            {contract::stateDir(workspace_, Status::Queued), Status::Queued},
        };

        for (const auto& [dir, status] : dirs) {
            auto latest = latestInDir(dir);
            if (!latest.has_value()) {
                continue;
            }
            latest->status = status;
            if (!newest.has_value() || latest->timestamp > newest->timestamp) {
                newest = latest;
            }
        }
        return newest;
    } catch (...) {
        return std::nullopt;
    }
}

bool Flow::isValidJobId(const JobId& id) noexcept {
    return contract::isValidJobId(id);
}

std::optional<Job> Flow::get(const JobId& id) const noexcept {
    try {
        if (!isValidJobId(id)) return std::nullopt;
        Status jobStatus = status(id);

        if (jobStatus == Status::Done) {
            auto outputDir = contract::jobDir(workspace_, Status::Done, id);
            auto artifact = contract::findOutputArtifact(outputDir);
            if (!artifact) {
                LOG_DEBUG("No result file found for job: " + id);
                return std::nullopt;
            }

            std::string content;
            switch (artifact->kind) {
                case contract::ArtifactKind::Result:
                    content = readResultContent(id);
                    break;
                case contract::ArtifactKind::Transcript: {
                    std::ifstream file(artifact->path, std::ios::binary);
                    content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    break;
                }
                case contract::ArtifactKind::Audio:
                    // Return the absolute audio path instead of binary content.
                    content = std::filesystem::absolute(artifact->path).string();
                    break;
                case contract::ArtifactKind::Embedding: {
                    std::ifstream file(artifact->path);
                    std::string line;
                    while (std::getline(file, line)) {
                        content += line + "\n";
                    }
                    break;
                }
            }

            auto timestamp = std::filesystem::last_write_time(outputDir);
            auto sctp = toSystemTime(timestamp);

            return Job{id, Status::Done, content, sctp};

        } else if (jobStatus == Status::Failed) {
            auto failedDir = contract::jobDir(workspace_, Status::Failed, id);
            auto errorFile = failedDir / contract::kErrorFile;

            std::string errorContent = "";
            if (std::filesystem::exists(errorFile)) {
                std::ifstream file(errorFile);
                std::string line;
                while (std::getline(file, line)) {
                    errorContent += line + "\n";
                }
            }

            auto timestamp = std::filesystem::last_write_time(failedDir);
            auto sctp = toSystemTime(timestamp);
            return Job{id, Status::Failed, errorContent, sctp};

        } else if (jobStatus == Status::Queued || jobStatus == Status::Running) {
            auto sctp = std::chrono::system_clock::now();
            return Job{id, jobStatus, "", sctp};
        }

        return std::nullopt;

    } catch (const std::exception& e) {
        LOG_ERROR("Error retrieving job " + id + ": " + e.what());
        return std::nullopt;
    }
}

std::vector<Job> Flow::list(std::size_t max) const noexcept {
    std::vector<Job> jobs;
    try {
        const std::pair<std::filesystem::path, Status> dirs[] = {
            {contract::stateDir(workspace_, Status::Done),    Status::Done},
            {contract::stateDir(workspace_, Status::Failed),  Status::Failed},
            {contract::stateDir(workspace_, Status::Running), Status::Running},
            {contract::stateDir(workspace_, Status::Queued),  Status::Queued},
        };

        for (const auto& [dir, status] : dirs) {
            if (!std::filesystem::exists(dir)) continue;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    std::string id = entry.path().filename().string();
                    if (!isValidJobId(id)) continue;
                    auto ts = std::filesystem::last_write_time(entry);
                    auto sctp = toSystemTime(ts);
                    jobs.push_back({id, status, "", sctp});
                }
            }
        }

        std::sort(jobs.begin(), jobs.end(), [](const Job& a, const Job& b) {
            return a.timestamp > b.timestamp;
        });

        if (jobs.size() > max) {
            jobs.resize(max);
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error listing jobs: " + std::string(e.what()));
    }

    return jobs;
}

Status Flow::status(const JobId& id) const noexcept {
    try {
        if (!isValidJobId(id)) return Status::Missing;
        // Check upstream states first: jobs move queued → running → done/failed,
        // so a mid-check rename is re-observed downstream instead of reading as
        // Missing (which callers like `flw -w` treat as terminal).
        if (std::filesystem::exists(contract::jobDir(workspace_, Status::Queued, id))) {
            return Status::Queued;
        }
        if (std::filesystem::exists(contract::jobDir(workspace_, Status::Running, id))) {
            return Status::Running;
        }
        if (std::filesystem::exists(contract::jobDir(workspace_, Status::Done, id))) {
            return Status::Done;
        }
        if (std::filesystem::exists(contract::jobDir(workspace_, Status::Failed, id))) {
            return Status::Failed;
        }

        return Status::Missing;

    } catch (...) {
        return Status::Missing;
    }
}

bool Flow::exists(const JobId& id) const noexcept {
    return status(id) != Status::Missing;
}

std::optional<std::string> Flow::error(const JobId& id) const {
    try {
        if (!isValidJobId(id)) return std::nullopt;
        auto errorFile = contract::jobDir(workspace_, Status::Failed, id) / contract::kErrorFile;
        if (!std::filesystem::exists(errorFile)) {
            return std::nullopt;
        }

        std::ifstream file(errorFile);
        std::string content, line;
        while (std::getline(file, line)) {
            content += line + "\n";
        }
        return content;

    } catch (const std::exception& e) {
        LOG_ERROR("Error reading error file for job " + id + ": " + e.what());
        return std::nullopt;
    }
}

std::optional<std::string> Flow::prompt(const JobId& id) const {
    try {
        if (!isValidJobId(id)) return std::nullopt;
        std::vector<std::filesystem::path> searchDirs = {
            contract::jobDir(workspace_, Status::Done, id),
            contract::jobDir(workspace_, Status::Failed, id),
            contract::jobDir(workspace_, Status::Running, id),
            contract::jobDir(workspace_, Status::Queued, id),
            workspace_ / contract::kWritingDir / id
        };

        for (const auto& dir : searchDirs) {
            auto promptFile = dir / contract::kPromptFile;
            if (std::filesystem::exists(promptFile)) {
                std::ifstream file(promptFile);
                std::string content, line;
                while (std::getline(file, line)) {
                    content += line + "\n";
                }
                return content;
            }
        }

        return std::nullopt;

    } catch (const std::exception& e) {
        LOG_ERROR("Error reading prompt file for job " + id + ": " + e.what());
        return std::nullopt;
    }
}

std::optional<JobMeta> Flow::meta(const JobId& id) const noexcept {
    try {
        if (!isValidJobId(id)) return std::nullopt;
        std::vector<std::filesystem::path> searchDirs = {
            contract::jobDir(workspace_, Status::Done, id),
            contract::jobDir(workspace_, Status::Failed, id),
            contract::jobDir(workspace_, Status::Running, id),
            contract::jobDir(workspace_, Status::Queued, id),
            workspace_ / contract::kWritingDir / id
        };

        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                auto meta = readMetaJson(dir);
                if (meta.has_value()) {
                    return meta;
                }
            }
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

static std::size_t countSubdirs(const std::filesystem::path& dir) noexcept {
    std::size_t n = 0;
    try {
        if (!std::filesystem::exists(dir)) return 0;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_directory()) {
                std::string id = entry.path().filename().string();
                if (Flow::isValidJobId(id)) ++n;
            }
        }
    } catch (...) {}
    return n;
}

WorkspaceCounts Flow::counts() const noexcept {
    WorkspaceCounts c;
    c.queued  = Scanner(workspace_).readyJobCount();
    c.running = countSubdirs(contract::stateDir(workspace_, Status::Running));
    c.done    = countSubdirs(contract::stateDir(workspace_, Status::Done));
    c.failed  = countSubdirs(contract::stateDir(workspace_, Status::Failed));
    return c;
}

std::optional<Job> Flow::latestInDir(const std::filesystem::path& dir) const noexcept {
    try {
        if (!std::filesystem::exists(dir)) return std::nullopt;
        std::optional<Job> newest;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            auto jobId = entry.path().filename().string();
            if (!isValidJobId(jobId)) continue;
            auto ts = toSystemTime(std::filesystem::last_write_time(entry));
            if (!newest || ts > newest->timestamp) {
                newest = Job{jobId, Status::Missing, "", ts};
            }
        }
        return newest;
    } catch (...) {
        return std::nullopt;
    }
}

std::string Flow::readResultContent(const JobId& id) const {
    auto resultFile = contract::jobDir(workspace_, Status::Done, id) / contract::kResultFile;
    std::ifstream file(resultFile);
    std::string content, line;
    while (std::getline(file, line)) {
        if (!content.empty()) content += "\n";
        content += line;
    }
    return content;
}

}
