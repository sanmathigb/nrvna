/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/
#pragma once
#include <string>

namespace nrvna {
class Flow {
    struct JobInference {
        std::string jobId;
        std::string inference;
    };
    public:
        Flow();
        explicit Flow(const std::string& outputLocation);
        JobInference retrieve();
        JobInference retrieve(const std::string& jobId);

    private:
        std::string outputLocation_;
        std::string findLatestJobId();
    };
}