/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#include "nrvna/flow.hpp"
#include <fstream>

namespace nrvna {
Flow::Flow() : outputLocation_("./nrvna_output") {}
Flow::Flow(const std::string& outputLocation) : outputLocation_(outputLocation) {}

JobInference Flow::retrieve(const std::string& jobId) {
    std::string filePath  = outputLocation_ + "/" + jobId + ".txt";
    std::ifstream file(filePath);
    std::string content((std::istreambuf_iterator<char>(file)),
    std::istreambuf_iterator<char>());
    return JobInference{jobId, content};
}

JobInference Flow::retrieve() {
    auto latestId = findLatestJobId();
    return retrieve(latestId);
}

std::string Flow::findLatestJobId() const {
    std::string latestJob = "";
    try {
        for (const auto& entry : std::filesystem::directory_iterator(outputLocation_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                std::string filename = entry.path().stem().string();
                if (filename.find("job") == 0) {  // Starts with "job"
                    if (latestJob.empty() || filename > latestJob) {  // Lexicographic comparison
                        latestJob = filename;
                    }
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }
    return latestJob;
}
}