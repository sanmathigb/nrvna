/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/
#include "nrvna/monitor.hpp"
#include "nrvna/runner.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>

namespace nrvna {

    Monitor::Monitor(const std::string& modelPath, const std::string& workspace)
        : model_path_(modelPath), workspace_(workspace) {
    }

    Monitor::~Monitor() {
        stop();
    }

    bool Monitor::start() {
        if (runner_) {
            std::cerr << "Monitor already started" << std::endl;
            return false;
        }

        if (!setupDirectories()) {
            return false;
        }

        try {
            runner_ = std::make_unique<Runner>(model_path_);
            std::cout << "Monitor started - Model loaded: " << model_path_ << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load model: " << e.what() << std::endl;
            return false;
        }
    }

    void Monitor::stop() {
        if (runner_) {
            runner_.reset();
            std::cout << "Monitor stopped - Model unloaded" << std::endl;
        }
    }

    bool Monitor::setupDirectories() {
        try {
            std::filesystem::create_directories(workspace_ + "/input");
            std::filesystem::create_directories(workspace_ + "/output");
            std::filesystem::create_directories(workspace_ + "/processing");
            std::filesystem::create_directories(workspace_ + "/failed");
            std::cout << "Monitor directories ready" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to setup directories: " << e.what() << std::endl;
            return false;
        }
    }

    // ✅ NEW: Just add these 3 methods
    int Monitor::processJobs() {
        if (!runner_) return 0;

        auto jobs = findJobs();
        int processed = 0;

        for (const auto& jobId : jobs) {
            if (processJob(jobId)) {
                processed++;
            }
        }

        return processed;
    }

    std::vector<std::string> Monitor::findJobs() const {
        std::vector<std::string> jobs;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(workspace_ + "/input")) {
                if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                    jobs.push_back(entry.path().stem().string());
                }
            }
        } catch (...) {}

        return jobs;
    }

    bool Monitor::processJob(const std::string& jobId) {
        std::string inputFile = workspace_ + "/input/" + jobId + ".txt";
        std::string processingFile = workspace_ + "/processing/" + jobId + ".txt";
        std::string outputFile = workspace_ + "/output/" + jobId + ".txt";

        try {
            std::filesystem::rename(inputFile, processingFile);

            std::ifstream file(processingFile);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            std::string result = runner_->run(content);

            std::ofstream outFile(outputFile);
            outFile << result;

            std::filesystem::remove(processingFile);
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error processing job " << jobId << ": " << e.what() << std::endl;
            try {
                std::filesystem::rename(processingFile, workspace_ + "/failed/" + jobId + ".txt");
            } catch (...) {}
            return false;
        }
    }

} // namespace nrvna