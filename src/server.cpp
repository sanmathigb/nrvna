/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/
#include "nrvna/server.hpp"
#include "nrvna/monitor.hpp"  // ✅ Uncommented
#include <filesystem>
#include <iostream>

namespace nrvna {

    Server::Server(const std::string& modelPath, const std::string& workspace)
        : model_path_(modelPath), workspace_(workspace) {
    }

    Server::~Server() {
        stop();
    }

    bool Server::start() {
        if (monitor_) {
            std::cerr << "Server already started" << std::endl;
            return false;
        }

        if (!setupWorkspace()) {
            return false;
        }

        // ✅ Actually create Monitor
        monitor_ = std::make_unique<Monitor>(model_path_, workspace_);
        return monitor_->start();  // This will load the AI model
    }

    void Server::stop() {
        if (monitor_) {
            monitor_->stop();
            monitor_.reset();
            std::cout << "Server stopped" << std::endl;
        }
    }

    bool Server::setupWorkspace() {
        try {
            std::filesystem::create_directories(workspace_ + "/input");
            std::filesystem::create_directories(workspace_ + "/output");
            std::filesystem::create_directories(workspace_ + "/processing");
            std::cout << "Workspace setup complete: " << workspace_ << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to setup workspace: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace nrvna