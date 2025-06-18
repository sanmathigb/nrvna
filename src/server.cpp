#include "nrvna/server.hpp"
#include "nrvna/monitor.hpp"
#include <filesystem>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>

namespace nrvna {

    std::atomic<bool> Server::shutdown_requested_{false};

    Server::Server(const std::string& modelPath, const std::string& workspace)
        : model_path_(modelPath), workspace_(workspace) {
    }

    Server::~Server() {
        stop();
    }

    bool Server::start() {
        if (monitor_ || running_) {
            std::cerr << "Server already started" << std::endl;
            return false;
        }

        if (!setup()) return false;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        monitor_ = std::make_unique<Monitor>(model_path_, workspace_);

        if (!monitor_->start()) {
            std::cerr << "Failed to start monitor" << std::endl;
            return false;
        }

        running_ = true;
        return true;
    }

    void Server::stop() {
        if (running_) running_ = false;
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
            std::cerr << "Model file not found: " << model_path_ << std::endl;
            return false;
        }

        try {
            std::filesystem::create_directories(workspace_);
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

    void Server::signalHandler(int signal) {
        shutdown_requested_ = true;
    }

} // namespace nrvna