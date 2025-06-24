/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace nrvna {
    class Runner;

    class Monitor {
    public:
        explicit Monitor(const std::string& modelPath,
                        const std::string& workspace,
                        int numWorkers = 6);
        ~Monitor();

        bool start();
        void stop();
        int process();
        void run();

    private:
        std::string model_path_;
        std::string workspace_;
        int numWorkers_;
        std::vector<std::unique_ptr<Runner>> runners_;

        std::atomic<bool> running_;
        std::vector<std::thread> workers_;
        std::thread monitor_thread_;

        std::queue<std::string> job_queue_;
        std::mutex queue_mutex_;
        std::condition_variable job_available_;

        bool setup();
        std::vector<std::string> findJobs() const;
        bool processJob(const std::string& jobId, int workerId);
        void monitorDirectory();
        void workerFunction(int workerId);

        // Email functionality
        std::string readEmailFromMeta(const std::string& metaPath);
        void sendEmailNotification(const std::string& email, const std::string& jobId, const std::string& result);
    };
} // namespace nrvna