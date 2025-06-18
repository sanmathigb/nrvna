#include "nrvna/monitor.hpp"
#include "nrvna/runner.hpp"
#include "nrvna/logger.hpp"  // NEW: Thread-safe logging
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

namespace nrvna {

Monitor::Monitor(const std::string& modelPath, const std::string& workspace, int numWorkers)
    : model_path_(modelPath), workspace_(workspace), numWorkers_(numWorkers), running_(false) {
}

Monitor::~Monitor() {
    stop();
}

bool Monitor::start() {
    if (!runners_.empty() || running_) return false;
    if (!setup()) return false;

    try {
        runners_.reserve(numWorkers_);
        for (int i = 0; i < numWorkers_; ++i) {
            runners_.emplace_back(std::make_unique<Runner>(model_path_));
        }
        LOG_INFO("Monitor started with " + std::to_string(numWorkers_) + " workers");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start monitor: " + std::string(e.what()));
        return false;
    }
}

void Monitor::stop() {
    if (running_) {
        running_ = false;
        job_available_.notify_all();

        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        LOG_DEBUG("Monitor stopped");
    }
    runners_.clear();
}

void Monitor::run() {
    if (running_ || !workers_.empty()) return;
    if (runners_.empty()) return;

    running_ = true;

    monitor_thread_ = std::thread(&Monitor::monitorDirectory, this);

    workers_.reserve(numWorkers_);
    for (int i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&Monitor::workerFunction, this, i);
    }

    LOG_INFO("Started continuous monitoring");
}

void Monitor::monitorDirectory() {
    LOG_DEBUG("Watching " + workspace_ + "/input/ready/");

    while (running_) {
        try {
            std::string ready_dir = workspace_ + "/input/ready";
            std::string processing_dir = workspace_ + "/processing";

            for (const auto& entry : std::filesystem::directory_iterator(ready_dir)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
                    continue;
                }

                std::string filename = entry.path().filename().string();
                std::string jobId = entry.path().stem().string();
                std::string readyPath = ready_dir + "/" + filename;
                std::string processingPath = processing_dir + "/" + filename;

                // CRITICAL FIX: Atomic rename BEFORE queuing
                try {
                    std::filesystem::rename(readyPath, processingPath);

                    // Only if rename succeeded, add to queue
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex_);
                        job_queue_.push(jobId);
                    }
                    job_available_.notify_one();

                    LOG_DEBUG("Job " + jobId + " queued for processing");

                } catch (const std::filesystem::filesystem_error&) {
                    // Another monitor scan or manual intervention moved it
                    // This is fine, just skip
                }
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Directory scan error: " + std::string(e.what()));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Monitor::workerFunction(int workerId) {
    LOG_DEBUG("Worker " + std::to_string(workerId) + " started");

    while (running_) {
        std::string jobId;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            job_available_.wait(lock, [this] {
                return !running_ || !job_queue_.empty();
            });

            if (!running_ && job_queue_.empty()) break;

            if (!job_queue_.empty()) {
                jobId = job_queue_.front();
                job_queue_.pop();
            }
        }

        if (!jobId.empty()) {
            LOG_DEBUG("Worker " + std::to_string(workerId) + " processing " + jobId);
            if (processJob(jobId, workerId)) {
                LOG_DEBUG("Worker " + std::to_string(workerId) + " completed " + jobId);
            } else {
                LOG_ERROR("Worker " + std::to_string(workerId) + " failed " + jobId);
            }
        }
    }

    LOG_DEBUG("Worker " + std::to_string(workerId) + " stopped");
}

bool Monitor::processJob(const std::string& jobId, int workerId) {
    // File is already in processing directory!
    std::string processingPath = workspace_ + "/processing/" + jobId + ".txt";
    std::string outputPath = workspace_ + "/output/" + jobId + ".txt";
    std::string failedPath = workspace_ + "/failed/" + jobId + ".txt";

    try {
        // Read input
        std::ifstream inFile(processingPath);
        if (!inFile) {
            throw std::runtime_error("Cannot read processing file");
        }

        std::string content((std::istreambuf_iterator<char>(inFile)),
                           std::istreambuf_iterator<char>());
        inFile.close();

        // Run inference
        std::string result = runners_[workerId]->run(content);
        if (result.empty()) {
            throw std::runtime_error("Empty inference result");
        }

        // Write output
        std::ofstream outFile(outputPath);
        if (!outFile) {
            throw std::runtime_error("Cannot write output file");
        }
        outFile << result;
        outFile.close();

        // Cleanup processing file
        std::filesystem::remove(processingPath);
        return true;

    } catch (const std::exception& e) {
        // Move to failed directory
        try {
            std::filesystem::rename(processingPath, failedPath);
        } catch (...) {
            // Best effort cleanup
            try {
                std::filesystem::remove(processingPath);
            } catch (...) {}
        }
        return false;
    }
}

int Monitor::process() {
    if (runners_.empty()) return 0;

    // For single-shot processing, work directly on ready directory
    std::string ready_dir = workspace_ + "/input/ready";
    std::string processing_dir = workspace_ + "/processing";
    int processed = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(ready_dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
                continue;
            }

            std::string filename = entry.path().filename().string();
            std::string jobId = entry.path().stem().string();
            std::string readyPath = ready_dir + "/" + filename;
            std::string processingPath = processing_dir + "/" + filename;

            try {
                std::filesystem::rename(readyPath, processingPath);
                if (processJob(jobId, 0)) {
                    processed++;
                }
            } catch (...) {
                // Skip if can't move
            }
        }
    } catch (...) {}

    return processed;
}

std::vector<std::string> Monitor::findJobs() const {
    // Not used anymore - kept for compatibility
    std::vector<std::string> jobs;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(workspace_ + "/input/ready")) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                jobs.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {}
    return jobs;
}

bool Monitor::setup() {
    try {
        std::filesystem::create_directories(workspace_ + "/input/writing");
        std::filesystem::create_directories(workspace_ + "/input/ready");
        std::filesystem::create_directories(workspace_ + "/output");
        std::filesystem::create_directories(workspace_ + "/processing");
        std::filesystem::create_directories(workspace_ + "/failed");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create directories: " + std::string(e.what()));
        return false;
    }
}

} // namespace nrvna