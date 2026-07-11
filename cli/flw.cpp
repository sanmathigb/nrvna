/*
 * nrvna ai - Flow retrieval tool (flw)
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/flow.hpp"
#include "nrvna/contract.hpp"
#include "nrvna/meta.hpp"
#include "nrvna/work.hpp"
#include "nrvna/logger.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <unistd.h>

using namespace nrvnaai;

constexpr const char* VERSION = NRVNA_VERSION;

void printUsage() {
    std::cout << "Read results and workspace status.\n\n";
    std::cout << "Usage:\n";
    std::cout << "  flw <workspace> [job_id] [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -w, --wait        Wait for a job\n";
    std::cout << "  -W, --wait-idle   Wait for the workspace to become idle\n";
    std::cout << "      --json        Print JSON\n";
    std::cout << "      --tag <t>       Select all jobs with tag (ids; with --json, NDJSON)\n";
    std::cout << "      --children <id> Select all jobs with parent <id>\n";
    std::cout << "  -h, --help        Show help\n";
    std::cout << "  -v, --version     Show version\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  flw ./ws                          workspace status\n";
    std::cout << "  flw ./ws <job-id>                 print a job's result\n";
    std::cout << "  wrk ./ws \"prompt\" | flw ./ws -w   submit and wait in one pipe\n";
    std::cout << "  flw ./ws -W                       block until all jobs finish\n";
    std::cout << "  flw ./ws --tag nightly --json | jq .   collect a whole batch\n";
    std::cout << "  flw ./ws -W --tag nightly         wait for YOUR jobs, not the world's\n";
    std::cout << "\n";
    std::cout << "Exit codes: 0 done, 1 failed, 2 not ready\n";
}

std::string readFileRaw(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

const char* statusToString(Status status) {
    switch (status) {
        case Status::Queued: return "QUEUED";
        case Status::Running: return "RUNNING";
        case Status::Done: return "DONE";
        case Status::Failed: return "FAILED";
        case Status::Missing: return "MISSING";
        default: return "UNKNOWN";
    }
}

const char* statusToJsonString(Status status) {
    return contract::toString(status);
}

// A job selected by --tag or --children, wherever it currently lives.
struct SetMatch {
    JobId id;
    Status status;
};

std::vector<SetMatch> selectSet(const std::filesystem::path& ws,
                                const std::string& tag, const std::string& parent) {
    // Scan upstream states first (queued → running → done → failed): jobs
    // move downstream between scans, so a mid-scan transition is re-observed
    // in a later directory instead of slipping through. Later sightings win.
    std::map<JobId, Status> found;
    for (Status s : {Status::Queued, Status::Running, Status::Done, Status::Failed}) {
        auto dir = contract::stateDir(ws, s);
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || ec) continue;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            auto id = entry.path().filename().string();
            if (!contract::isValidJobId(id)) continue;
            auto meta = readMetaJson(entry.path());
            if (!meta) continue;
            bool hit = false;
            if (!tag.empty())
                hit = std::find(meta->tags.begin(), meta->tags.end(), tag) != meta->tags.end();
            if (!parent.empty())
                hit = hit || meta->parent == parent;
            if (hit) found[id] = s;
        }
    }
    std::vector<SetMatch> matches;
    matches.reserve(found.size());
    for (const auto& [id, s] : found) matches.push_back({id, s});
    return matches;  // std::map iterates id-sorted
}

// One JSON object for one job — the single-job --json shape, reused per
// NDJSON line for sets. Returns the exit code the single-job path uses.
int printJobJson(Flow& flow, const std::filesystem::path& wsPath, const Job& job) {
    auto meta = flow.meta(job.id);
    auto outputDir = contract::jobDir(wsPath, Status::Done, job.id);
    std::ostringstream out;
    out << "{";
    out << "\"id\":\"" << escapeJson(job.id) << "\"";
    out << ",\"status\":\"" << escapeJson(std::string(statusToJsonString(job.status))) << "\"";
    if (meta) {
        if (!meta->mode.empty()) out << ",\"mode\":\"" << escapeJson(meta->mode) << "\"";
        if (!meta->submitted_at.empty()) out << ",\"submitted_at\":\"" << escapeJson(meta->submitted_at) << "\"";
        if (!meta->completed_at.empty()) out << ",\"completed_at\":\"" << escapeJson(meta->completed_at) << "\"";
        if (meta->duration_s >= 0.0) out << ",\"duration_s\":" << meta->duration_s;
        if (!meta->parent.empty()) out << ",\"parent\":\"" << escapeJson(meta->parent) << "\"";
        if (!meta->tags.empty()) {
            out << ",\"tags\":[";
            for (size_t i = 0; i < meta->tags.size(); ++i) {
                if (i > 0) out << ",";
                out << "\"" << escapeJson(meta->tags[i]) << "\"";
            }
            out << "]";
        }
        if (!meta->artifacts.empty()) {
            out << ",\"artifacts\":[";
            for (size_t i = 0; i < meta->artifacts.size(); ++i) {
                if (i > 0) out << ",";
                out << "\"" << escapeJson(meta->artifacts[i]) << "\"";
            }
            out << "]";
        }
    }

    if (job.status == Status::Done) {
        if (auto artifact = contract::findOutputArtifact(outputDir)) {
            out << ",\"artifact_kind\":\"" << contract::toString(artifact->kind) << "\"";
            out << ",\"artifact_path\":\"" << escapeJson(std::filesystem::absolute(artifact->path).string()) << "\"";
            switch (artifact->kind) {
                case contract::ArtifactKind::Result:
                    out << ",\"result\":\"" << escapeJson(job.content) << "\"";
                    break;
                case contract::ArtifactKind::Transcript:
                    out << ",\"transcript\":\"" << escapeJson(job.content) << "\"";
                    break;
                case contract::ArtifactKind::Audio:
                    out << ",\"audio_path\":\"" << escapeJson(std::filesystem::absolute(artifact->path).string()) << "\"";
                    break;
                case contract::ArtifactKind::Embedding: {
                    // Compact: embedding.json is pretty-printed on disk, but
                    // NDJSON requires one object per line.
                    auto raw = readFileRaw(artifact->path);
                    raw.erase(std::remove(raw.begin(), raw.end(), '\n'), raw.end());
                    out << ",\"embedding\":" << raw;
                    break;
                }
            }
        }
    } else if (job.status == Status::Failed) {
        out << ",\"error\":\"" << escapeJson(job.content) << "\"";
    }
    out << "}\n";
    std::cout << out.str();
    return job.status == Status::Failed ? 1 : 0;
}

int main(int argc, char* argv[]) {
    // Default to WARN for clean output; NRVNA_LOG_LEVEL overrides
    if (!std::getenv("NRVNA_LOG_LEVEL"))
        Logger::setLevel(LogLevel::WARN);

    // Handle --help and --version before anything else
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << VERSION << "\n";
            return 0;
        }
    }

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string workspace = argv[1];
    std::string jobId = "";
    std::string selectTag, selectParent;
    bool wait = false;
    bool waitIdle = false;
    bool json = false;

    // Parse args
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w" || arg == "--wait") {
            wait = true;
        } else if (arg == "--wait-idle" || arg == "-W") {
            waitIdle = true;
        } else if (arg == "--json") {
            json = true;
        } else if (arg == "--tag") {
            if (i + 1 >= argc) { std::cerr << "Error: --tag requires a value\n"; return 1; }
            selectTag = argv[++i];
        } else if (arg == "--children") {
            if (i + 1 >= argc) { std::cerr << "Error: --children requires a job ID\n"; return 1; }
            selectParent = argv[++i];
        } else {
            jobId = arg;
        }
    }

    // Check piped input for JobID if not provided
    if (wait && jobId.empty() && !isatty(fileno(stdin))) {
        if (!(std::cin >> jobId)) {
            std::cerr << "No job ID received on stdin" << std::endl;
            return 1;
        }
    }

    // Validate job ID format
    if (!jobId.empty() && !Flow::isValidJobId(jobId)) {
        std::cerr << "Invalid job ID: " << jobId << std::endl;
        return 1;
    }
    if (!selectParent.empty() && !Flow::isValidJobId(selectParent)) {
        std::cerr << "Invalid parent job ID: " << selectParent << std::endl;
        return 1;
    }
    if (!selectTag.empty() && !Work::isValidTag(selectTag)) {
        std::cerr << "Invalid tag: " << selectTag << std::endl;
        return 1;
    }

    try {
        Flow flow(workspace);
        std::filesystem::path wsPath(workspace);

        // Set output: all jobs matching --tag / --children. JSON collection
        // aggregates failure: exit 1 if any job in the set failed or could
        // not be retrieved, so batch scripts can trust the exit code.
        if ((!selectTag.empty() || !selectParent.empty()) && !waitIdle) {
            auto matches = selectSet(wsPath, selectTag, selectParent);
            int rc = 0;
            for (const auto& m : matches) {
                if (json) {
                    auto job = flow.get(m.id);
                    if (!job || printJobJson(flow, wsPath, *job) != 0) rc = 1;
                } else {
                    std::cout << m.id << "\n";
                }
            }
            return json ? rc : 0;
        }

        // Scoped wait: block until no queued/running job matches the selection;
        // fail if the set contains failures. Bare -W keeps global-idle semantics.
        if (waitIdle && (!selectTag.empty() || !selectParent.empty())) {
            while (true) {
                auto set = selectSet(wsPath, selectTag, selectParent);
                bool pending = false, failed = false;
                for (const auto& m : set) {
                    if (m.status == Status::Queued || m.status == Status::Running) pending = true;
                    if (m.status == Status::Failed) failed = true;
                }
                if (!pending) return failed ? 1 : 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }

        // Wait for workspace idle
        if (waitIdle) {
            while (true) {
                auto c = flow.counts();
                if (c.queued == 0 && c.running == 0) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (json) {
                auto c = flow.counts();
                std::cout << "{\"queued\":" << c.queued
                          << ",\"running\":" << c.running
                          << ",\"done\":" << c.done
                          << ",\"failed\":" << c.failed << "}\n";
                return c.failed > 0 ? 1 : 0;
            }
            auto c = flow.counts();
            std::cout << "idle (" << c.done << " done, " << c.failed << " failed)\n";
            return c.failed > 0 ? 1 : 0;
        }

        // No job ID and no pipe: show workspace status
        if (jobId.empty() && !wait) {
            auto c = flow.counts();
            if (json) {
                std::cout << "{\"queued\":" << c.queued
                          << ",\"running\":" << c.running
                          << ",\"done\":" << c.done
                          << ",\"failed\":" << c.failed << "}\n";
                return 0;
            }

            std::cout << "queued:     " << c.queued << "\n"
                      << "running:    " << c.running << "\n"
                      << "done:       " << c.done << "\n"
                      << "failed:     " << c.failed << "\n";

            if (c.queued + c.running + c.done + c.failed == 0) {
                std::cout << "\nno jobs yet — submit one:  wrk " << workspace << " \"your prompt\"\n";
                return 0;
            }

            // Show recent jobs with duration from meta.json
            auto recentJobs = flow.list(5);
            if (!recentJobs.empty()) {
                std::cout << "\nrecent:\n";
                for (const auto& job : recentJobs) {
                    const char* tag = statusToJsonString(job.status);
                    auto m = flow.meta(job.id);
                    if (m && m->duration_s >= 0.0) {
                        char dur[16];
                        std::snprintf(dur, sizeof(dur), "%5.1fs", m->duration_s);
                        std::cout << "  [" << tag << "] " << dur << "  " << job.id << "\n";
                    } else {
                        std::cout << "  [" << tag << "]        " << job.id << "\n";
                    }
                }
            }
            return 0;
        }

        // Resolve ID (Specific or Latest)
        if (jobId.empty()) {
             auto latest = flow.latest();
             if (latest) jobId = latest->id;
             else {
                 std::cerr << "No jobs found" << std::endl;
                 return 1;
             }
        }

        // Wait loop
        if (wait) {
            while (true) {
                Status s = flow.status(jobId);
                if (s == Status::Done || s == Status::Failed || s == Status::Missing) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (!jobId.empty()) {
            // Retrieve specific job
            auto job = flow.get(jobId);
            
            if (!job.has_value()) {
                std::cerr << "Job not found: " << jobId << std::endl;
                return 1;
            }

            if (json) {
                return printJobJson(flow, wsPath, *job);
            }

            if (job->status == Status::Done) {
                // Audio output — print path instead of binary content
                auto artifact = contract::findOutputArtifact(
                    contract::jobDir(std::filesystem::path(workspace), Status::Done, jobId));
                if (artifact && artifact->kind == contract::ArtifactKind::Audio) {
                    std::cout << std::filesystem::absolute(artifact->path).string() << std::endl;
                    return 0;
                }
                std::cout << job->content;
                if (!job->content.empty() && job->content.back() != '\n') {
                    std::cout << '\n';
                }
                return 0;
            } else if (job->status == Status::Failed) {
                std::cerr << "Job failed: " << jobId << std::endl;
                if (!job->content.empty()) {
                    std::cerr << "Error: " << job->content << std::endl;
                }
                return 1;
            } else {
                std::cerr << "Job not ready: " << jobId << " (status: " << statusToString(job->status) << ")" << std::endl;
                return 2; // Different exit code for "not ready"
            }

        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error: Unknown error retrieving job\n";
        return 1;
    }
}
