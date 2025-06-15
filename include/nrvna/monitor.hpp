/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <memory>
#include <vector>

namespace nrvna {
    class Runner;

    class Monitor {
    public:
        explicit Monitor(const std::string& modelPath,
                        const std::string& workspace);
        ~Monitor();

        bool start();
        void stop();

        // Job processing method - exactly matches implementation
        int processJobs();

    private:
        std::string model_path_;
        std::string workspace_;
        std::unique_ptr<Runner> runner_;

        // Private methods - exactly match implementation
        bool setupDirectories();
        std::vector<std::string> findJobs() const;
        bool processJob(const std::string& jobId);
    };
} // namespace nrvna