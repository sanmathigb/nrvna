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
    uint64_t latestTimestamp = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(outputLocation_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                std::string filename = entry.path().stem().string();
                if (filename.find("job") == 0) {
                    size_t first_underscore = filename.find('_');
                    size_t second_underscore = filename.find('_', first_underscore + 1);

                    if (first_underscore != std::string::npos && second_underscore != std::string::npos) {
                        std::string timestamp_str = filename.substr(first_underscore + 1,
                                                                   second_underscore - first_underscore - 1);
                        try {
                            uint64_t timestamp = std::stoull(timestamp_str);
                            if (timestamp > latestTimestamp) {
                                latestTimestamp = timestamp;
                                latestJob = filename;
                            }
                        } catch (...) {}
                    }
                }
            }
        }
    } catch (...) {}
    return latestJob;
}
}