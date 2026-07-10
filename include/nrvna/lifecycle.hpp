/*
 * nrvna ai - Asynchronous Inference Primitive
 * Copyright (c) 2025 Sanmathi Bharamgouda
 * SPDX-License-Identifier: MIT
 *
 * The daemon lifecycle contract. nrvnad owns these files; this module is
 * the blessed reader/writer. Deliberately separate from the job contract
 * (nrvna/contract.hpp) — see CONTEXT.md.
 */
#pragma once
#include "nrvna/flow.hpp"
#include <cstdint>
#include <filesystem>
#include <string>

namespace nrvnaai::lifecycle {

inline constexpr const char* kLockFile  = ".nrvnad.lock";
inline constexpr const char* kPidFile   = ".nrvnad.pid";
inline constexpr const char* kReadyFile = ".nrvnad.ready";
inline constexpr const char* kInfoFile  = ".nrvnad.info";

enum class DaemonState : uint8_t { NotRunning, Starting, Ready };

struct DaemonInfo {
    DaemonState state = DaemonState::NotRunning;
    int pid = 0;
    std::string model;
    std::string mmproj;
    std::string vocoder;
    std::string started_at;
    int workers = 0;
};

// Client side: probe the flock (the single liveness truth), read the files.
// Cleans up stale pid/ready/info files when no daemon holds the lock.
[[nodiscard]] DaemonInfo query(const std::filesystem::path& ws);

// Client side: SIGTERM the daemon, wait for the lock to release, escalate
// (second TERM forces nrvnad's fast exit path), verify. 0 = stopped or was
// not running; 1 = still holding the workspace after timeout.
[[nodiscard]] int stopDaemon(const std::filesystem::path& ws, int timeoutSeconds = 20);

// Client side: block until the workspace queue is quiet. Returns final counts.
[[nodiscard]] WorkspaceCounts waitForIdle(const std::filesystem::path& ws, int pollMs = 500);

// Daemon side: written after the model is loaded; removed on clean shutdown.
[[nodiscard]] bool writeRuntimeFiles(const std::filesystem::path& ws, const DaemonInfo& info);
void removeRuntimeFiles(const std::filesystem::path& ws);

} // namespace nrvnaai::lifecycle
