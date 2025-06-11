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
    std::filesystem::create_directories(inputLocation_);
}

Work::Work(const std::string& inputLocation) : inputLocation_(inputLocation) {
    std::filesystem::create_directories(inputLocation_);
}

std::string Work::submit(const std::string& content) {
    if (content.empty()) { return ""; }
    std::string jobId = generateJobId();
    std::string inputFile = inputLocation_ + "/" + jobId + ".txt";
    std::ofstream file(inputFile);
    file << content;
    file.close();
    return jobId;
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
}