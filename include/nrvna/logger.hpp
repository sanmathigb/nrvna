#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <mutex>

// Log levels
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    void setLevel(LogLevel level) {
        m_currentLogLevel = level;
    }

    LogLevel getLevel() const {
        return m_currentLogLevel;
    }

    void log(LogLevel level, const std::string& message) const {
        if (level >= m_currentLogLevel) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            struct tm buf;
#ifdef _WIN32
            localtime_s(&buf, &in_time_t);
#else
            localtime_r(&in_time_t, &buf);
#endif

            std::string level_str;
            switch (level) {
                case LogLevel::DEBUG: level_str = "DEBUG"; break;
                case LogLevel::INFO:  level_str = "INFO";  break;
                case LogLevel::WARN:  level_str = "WARN";  break;
                case LogLevel::ERROR: level_str = "ERROR"; break;
            }
            
            std::cerr << "[" << std::put_time(&buf, "%Y-%m-%d %X") 
                      << "] [" << level_str << "] " << message << std::endl;
        }
    }

private:
    Logger() : m_currentLogLevel(LogLevel::INFO) {}
    LogLevel m_currentLogLevel;
    mutable std::mutex m_mutex;
};

// Logging macros
#define LOG_DEBUG(msg) Logger::instance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  Logger::instance().log(LogLevel::INFO, msg)
#define LOG_WARN(msg)  Logger::instance().log(LogLevel::WARN, msg)
#define LOG_ERROR(msg) Logger::instance().log(LogLevel::ERROR, msg)

        
