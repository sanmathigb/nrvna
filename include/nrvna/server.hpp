/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <memory>

namespace nrvna {
    class Monitor;

    class Server {
    public:
        explicit Server(const std::string& modelPath,
                       const std::string& workspace = "./data");
        ~Server();

        bool start();
        void stop();

    private:
        std::string model_path_;
        std::string workspace_;
        std::unique_ptr<Monitor> monitor_;

        bool setupWorkspace();
    };
} // namespace nrvna