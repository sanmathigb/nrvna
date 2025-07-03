#include "nrvna/server.hpp"
#include "nrvna/monitor.hpp"
#include "nrvna/http_receiver.hpp"
#include "nrvna/logger.hpp"
#include <filesystem>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

namespace nrvna {

    // =============================================================================
    // Server class - your existing implementation (unchanged)
    // =============================================================================

    std::atomic<bool> Server::shutdown_requested_{false};

    Server::Server(const std::string& modelPath, const std::string& workspace)
        : model_path_(modelPath), workspace_(workspace) {
    }

    Server::~Server() {
        stop();
    }

    bool Server::start() {
        if (monitor_ || running_) {
            LOG_ERROR("Server already started");
            return false;
        }

        if (!setup()) return false;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        monitor_ = std::make_unique<Monitor>(model_path_, workspace_);

        if (!monitor_->start()) {
            LOG_ERROR("Failed to start monitor");
            return false;
        }

        // Start HTTP server
        try {
            http_server_ = std::make_unique<HttpReceiver>(8080, workspace_ + "/input");
            http_server_->start();
            LOG_INFO("HTTP server started - ready for phone connections!");
        } catch (const std::exception& e) {
            LOG_ERROR("HTTP server failed to start: " + std::string(e.what()));
            LOG_INFO("Monitor still running for local wrk/flw commands");
            // Continue without HTTP - local commands still work
        }

        running_ = true;
        return true;
    }

    void Server::stop() {
        if (running_) running_ = false;

        // Stop HTTP server
        if (http_server_) {
            http_server_->stop();
            http_server_.reset();
            LOG_INFO("HTTP server stopped");
        }

        if (monitor_) {
            monitor_->stop();
            monitor_.reset();
        }
    }

    bool Server::waitForShutdown() {
        if (!running_) return false;

        monitor_->run();
        while (running_ && !shutdown_requested_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        stop();
        return true;
    }

    bool Server::setup() {
        if (!std::filesystem::exists(model_path_)) {
            LOG_ERROR("Model file not found: " + model_path_);
            return false;
        }

        try {
            std::filesystem::create_directories(workspace_);
            std::filesystem::create_directories(workspace_ + "/input");
            std::filesystem::create_directories(workspace_ + "/input/ready");
            std::filesystem::create_directories(workspace_ + "/output");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR(e.what());
            return false;
        }
    }

    void Server::signalHandler(int signal) {
        shutdown_requested_ = true;
    }

    void Server::requestShutdown() {
        shutdown_requested_ = true;
    }

} // namespace nrvna