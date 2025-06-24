/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#include "nrvna/work.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sstream>

namespace nrvna {

Work::Work() : inputLocation_("./nrvna_input") {
    setupDirectories();
}

Work::Work(const std::string& inputLocation) : inputLocation_(inputLocation) {
    setupDirectories();
}

std::string Work::submit(const std::string& content) {
    return submit(content, "");
}

std::string Work::submit(const std::string& content, const std::string& email) {
    if (content.empty()) { return ""; }

    std::string jobId = generateJobId();

    // Step 1: Write content to writing/ directory
    std::string writingFile = inputLocation_ + "/writing/" + jobId + ".txt";
    std::ofstream file(writingFile);
    if (!file) {
        std::cerr << "Failed to create writing file: " << writingFile << std::endl;
        return "";
    }
    file << content;
    file.close();

    // Step 2: Write metadata if email provided
    if (!email.empty()) {
        std::string metaFile = inputLocation_ + "/writing/" + jobId + ".meta";
        std::ofstream meta(metaFile);
        if (meta) {
            meta << "email=" << email << std::endl;
            meta.close();
        }
    }

    // Step 3: Atomic move to ready/ directory
    std::string readyFile = inputLocation_ + "/ready/" + jobId + ".txt";
    try {
        std::filesystem::rename(writingFile, readyFile);

        // Move metadata file if it exists
        if (!email.empty()) {
            std::string writingMeta = inputLocation_ + "/writing/" + jobId + ".meta";
            std::string readyMeta = inputLocation_ + "/ready/" + jobId + ".meta";
            std::filesystem::rename(writingMeta, readyMeta);
        }

        std::cout << "Job submitted: " << jobId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to move job to ready: " << e.what() << std::endl;
        // Cleanup partial files
        std::filesystem::remove(writingFile);
        if (!email.empty()) {
            std::filesystem::remove(inputLocation_ + "/writing/" + jobId + ".meta");
        }
        return "";
    }

    return jobId;
}

void Work::setupDirectories() {
    try {
        std::filesystem::create_directories(inputLocation_ + "/writing");
        std::filesystem::create_directories(inputLocation_ + "/ready");
    } catch (const std::exception& e) {
        std::cerr << "Failed to setup work directories: " << e.what() << std::endl;
    }
}

std::string Work::generateJobId() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t unique_counter = counter.fetch_add(1);
    std::stringstream ss;
    ss << "job" << getpid() << "_" << now << "_" << unique_counter;
    return ss.str();
}

} // namespace nrvna