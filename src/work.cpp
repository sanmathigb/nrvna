/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/
#include "nrvna/work.hpp"
#include <filesystem>
#include <fstream>

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
   static int counter = 1;
   return "job" + std::to_string(counter++);
}

}