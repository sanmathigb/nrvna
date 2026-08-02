/*
 * nrvna - Durable Local Inference Primitives
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 */

#include "nrvna/lifecycle.hpp"
#include "nrvna/meta.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/param.h>
#endif

namespace nrvna::lifecycle {

namespace {

// Try to acquire the workspace lock. Returns the held fd (caller must
// unlock+close), or -1. daemonHoldsIt distinguishes "a daemon owns it"
// from "no lock file exists".
int tryAcquireLock(const std::filesystem::path& ws, bool& daemonHoldsIt) {
    daemonHoldsIt = false;
    auto lockPath = ws / kLockFile;
    int fd = ::open(lockPath.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return -1;                    // workspace unwritable/missing
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        daemonHoldsIt = true;                 // someone else holds it
        (void)::close(fd);
        return -1;
    }
    return fd;                                // we hold it now
}

// True if a daemon currently holds the workspace lock.
bool lockIsHeld(const std::filesystem::path& ws) {
    bool held = false;
    int fd = tryAcquireLock(ws, held);
    if (fd >= 0) {
        (void)::flock(fd, LOCK_UN);
        (void)::close(fd);
    }
    return held;
}

int readPidFile(const std::filesystem::path& ws) {
    std::ifstream f(ws / kPidFile);
    int pid = 0;
    f >> pid;
    return pid;
}

// The current holder writes its PID while it holds the lock. This PID is
// authoritative when .nrvnad.pid is stale.
int readLockHolderPid(const std::filesystem::path& ws) {
    std::ifstream f(ws / kLockFile);
    int pid = 0;
    f >> pid;
    return pid;
}

// Confirm that the PID belongs to nrvnad before sending a signal.
// The lock remains the source of liveness state.
bool pidLooksLikeNrvnad(int pid) {
#ifdef __APPLE__
    char name[2 * MAXCOMLEN] = {0};
    if (proc_name(pid, name, sizeof(name)) <= 0) return false;
    return std::string(name).find("nrvnad") != std::string::npos;
#else
    std::ifstream f("/proc/" + std::to_string(pid) + "/comm");
    std::string comm;
    std::getline(f, comm);
    return comm.find("nrvnad") != std::string::npos;
#endif
}

// A signal is safe only while the same nrvnad still owns this workspace.
bool workspaceOwnedByNrvnad(const std::filesystem::path& ws, int pid) {
    return pid > 0 && readLockHolderPid(ws) == pid &&
           lockIsHeld(ws) && pidLooksLikeNrvnad(pid);
}

// Minimal extraction from .nrvnad.info; tolerant of missing fields.
void parseInfo(const std::filesystem::path& ws, DaemonInfo& info) {
    std::ifstream f(ws / kInfoFile);
    if (!f) return;
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto field = [&s](const char* key) -> std::string {
        auto k = "\"" + std::string(key) + "\":\"";
        auto i = s.find(k);
        if (i == std::string::npos) return "";
        i += k.size();
        auto j = s.find('"', i);
        return j == std::string::npos ? "" : s.substr(i, j - i);
    };
    info.model = field("model");
    info.mmproj = field("mmproj");
    info.vocoder = field("vocoder");
    info.started_at = field("started_at");
    auto wk = s.find("\"workers\":");
    if (wk != std::string::npos) info.workers = std::atoi(s.c_str() + wk + 10);
}

} // namespace

bool daemonPresent(const std::filesystem::path& ws) {
    return lockIsHeld(ws);
}

DaemonInfo query(const std::filesystem::path& ws) {
    DaemonInfo info;
    std::error_code ec;
    if (!std::filesystem::exists(ws, ec) || ec) return info;

    bool daemonHoldsIt = false;
    int fd = tryAcquireLock(ws, daemonHoldsIt);
    if (!daemonHoldsIt) {
        // No daemon. Clean stale files WHILE holding the lock, so a daemon
        // starting concurrently can't have its fresh files deleted.
        if (fd >= 0) {
            std::filesystem::remove(ws / kPidFile, ec);
            std::filesystem::remove(ws / kReadyFile, ec);
            std::filesystem::remove(ws / kInfoFile, ec);
            (void)::flock(fd, LOCK_UN);
            (void)::close(fd);
        }
        return info;
    }

    info.pid = readPidFile(ws);
    if (std::filesystem::exists(ws / kReadyFile, ec) && !ec) {
        info.state = DaemonState::Ready;
        parseInfo(ws, info);
    } else {
        info.state = DaemonState::Starting;
    }
    return info;
}

int stopDaemon(const std::filesystem::path& ws, int timeoutSeconds) {
    auto info = query(ws);
    if (info.state == DaemonState::NotRunning) return 0;
    // The lock holder writes its PID while it holds the flock. Prefer this PID
    // over the separate PID file, which can be stale.
    if (int lockPid = readLockHolderPid(ws); lockPid > 0) info.pid = lockPid;
    if (info.pid <= 0) return 1;  // lock held but pid unknown: refuse to guess
    // Refuse when the PID does not identify the nrvnad process that owns this
    // workspace.
    if (!workspaceOwnedByNrvnad(ws, info.pid)) return 1;

    (void)::kill(info.pid, SIGTERM);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    bool escalated = false;
    while (lockIsHeld(ws)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            // Revalidate identity before every escalation signal. The flock
            // being held implies the holder is alive, but the pid we signal
            // must still be an nrvnad at the moment we signal it.
            if (!workspaceOwnedByNrvnad(ws, info.pid)) return 1;
            if (!escalated) {
                // nrvnad treats a second signal as "force exit now"
                (void)::kill(info.pid, SIGTERM);
                escalated = true;
                deadline += std::chrono::seconds(3);
            } else {
                (void)::kill(info.pid, SIGKILL);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (lockIsHeld(ws)) return 1;
    std::error_code ec;
    std::filesystem::remove(ws / kPidFile, ec);
    std::filesystem::remove(ws / kReadyFile, ec);
    std::filesystem::remove(ws / kInfoFile, ec);
    return 0;
}

WorkspaceCounts waitForIdle(const std::filesystem::path& ws, int pollMs) {
    Flow flow(ws);
    while (true) {
        auto c = flow.counts();
        if (c.queued == 0 && c.running == 0) return c;
        std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
    }
}

bool writeRuntimeFiles(const std::filesystem::path& ws, const DaemonInfo& info) {
    try {
        {
            std::ofstream f(ws / kInfoFile);
            if (!f) return false;
            f << "{\"pid\":" << info.pid
              << ",\"model\":\"" << escapeJson(info.model) << "\""
              << ",\"mmproj\":\"" << escapeJson(info.mmproj) << "\""
              << ",\"vocoder\":\"" << escapeJson(info.vocoder) << "\""
              << ",\"workers\":" << info.workers
              << ",\"started_at\":\"" << escapeJson(info.started_at) << "\"}\n";
        }
        {
            std::ofstream f(ws / kReadyFile);
            if (!f) return false;
            f << info.started_at << "\n";
        }
        return true;
    } catch (...) {
        return false;
    }
}

void removeRuntimeFiles(const std::filesystem::path& ws) {
    std::error_code ec;
    std::filesystem::remove(ws / kReadyFile, ec);
    std::filesystem::remove(ws / kInfoFile, ec);
}

} // namespace nrvna::lifecycle
