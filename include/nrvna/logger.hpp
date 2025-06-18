/*
* nrvna - Local AI Orchestration Engine
* Copyright (c) 2025 Sanmathi Bharamgouda
* SPDX-License-Identifier: MIT
*/

#pragma once
#include <string>
#include <mutex>
#include <iostream>

namespace nrvna {

    enum class LogLevel {
        ERROR = 0,
        INFO = 1,
        DEBUG = 2
    };

    class Logger {
    public:
        static Logger& instance() {
            static Logger logger;
            return logger;
        }

        void setLevel(LogLevel level) {
            level_ = level;
        }

        LogLevel getLevel() const {
            return level_;
        }

        // Thread-safe logging methods
        void error(const std::string& message) {
            log(LogLevel::ERROR, "ERROR", message);
        }

        void info(const std::string& message) {
            if (level_ >= LogLevel::INFO) {
                log(LogLevel::INFO, "INFO", message);
            }
        }

        void debug(const std::string& message) {
            if (level_ >= LogLevel::DEBUG) {
                log(LogLevel::DEBUG, "DEBUG", message);
            }
        }

    private:
        Logger() = default;
        std::mutex mutex_;
        LogLevel level_ = LogLevel::ERROR; // Default: only errors

        void log(LogLevel level, const std::string& prefix, const std::string& message) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (level == LogLevel::ERROR) {
                std::cerr << "[" << prefix << "] " << message << std::endl;
            } else {
                std::cout << "[" << prefix << "] " << message << std::endl;
            }
        }
    };

    // Convenient macros for clean code
#define LOG_ERROR(msg) nrvna::Logger::instance().error(msg)
#define LOG_INFO(msg) nrvna::Logger::instance().info(msg)
#define LOG_DEBUG(msg) nrvna::Logger::instance().debug(msg)

} // namespace nrvna