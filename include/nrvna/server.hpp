/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <memory>
#include <atomic>

namespace nrvna {
    class Monitor;
    class HttpReceiver;  // ← ADD THIS FORWARD DECLARATION

    class Server {
    public:
        explicit Server(const std::string& modelPath,
                       const std::string& workspace);
        ~Server();

        bool start();
        void stop();
        bool waitForShutdown();

    private:
        std::string model_path_;
        std::string workspace_;
        std::unique_ptr<Monitor> monitor_;
        std::unique_ptr<HttpReceiver> http_server_;  // ← ADD THIS MEMBER
        std::atomic<bool> running_{false};

        bool setup();
        static std::atomic<bool> shutdown_requested_;
        static void signalHandler(int signal);

    public:
        static void requestShutdown();
    };
} // namespace nrvna