/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>

namespace nrvna {
    class Work {
    public:
        Work();
        explicit Work(const std::string& inputLocation);

        std::string submit(const std::string& content);
        std::string submit(const std::string& content, const std::string& email);

    private:
        std::string inputLocation_;

        std::string generateJobId();
        void setupDirectories();
    };
}