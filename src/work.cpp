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
        configure();
    }
    Work::Work(const std::string& inputLocation) : inputLocation_(inputLocation) {
        configure();
    }
    std::string Work::submit(const std::string& content) {
        if (content.empty()) { return ""; }
        std::string jobId = generateJobId();
        std::string writingFile = inputLocation_ + "/writing/" + jobId + ".txt";
        std::ofstream file(writingFile);
        if (!file) {
            std::cerr << "Failed to create writing file: " << writingFile << std::endl;
            return "";
        }
        file << content;
        file.close();
        std::string readyFile = inputLocation_ + "/ready/" + jobId + ".txt";
        try {
            std::filesystem::rename(writingFile, readyFile);
            std::cout << "Job submitted: " << jobId << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to move job to ready: " << e.what() << std::endl;
            std::filesystem::remove(writingFile);
            return "";
        }
        return jobId;
    }
    void Work::configure() {
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