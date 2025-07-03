// Enhanced cli/server.cpp with better logging control

#include "nrvna/server.hpp"
#include "nrvna/logger.hpp"
#include <iostream>
#include <string>

using namespace nrvna;

void printUsage(const char* program) {
    std::cout << "usage: " << program << " <model> <workspace> [-v|-d]\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    // Default: Show only errors and critical startup info
    LogLevel logLevel = LogLevel::ERROR;

    // Parse logging flags
    if (argc > 3) {
        std::string flag = argv[3];
        if (flag == "-q" || flag == "--quiet") {
            logLevel = LogLevel::ERROR;
        } else if (flag == "-v" || flag == "--verbose") {
            logLevel = LogLevel::INFO;
        } else if (flag == "-d" || flag == "--debug") {
            logLevel = LogLevel::DEBUG;
        } else if (flag == "-h" || flag == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "unknown option: " << flag << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Set logging level
    Logger::instance().setLevel(logLevel);

    // Always show startup message (even in quiet mode)
    std::cout << "zero: ready\n";

    // Start the server
    Server server(argv[1], argv[2]);
    if (!server.start()) {
        std::cerr << "failed to start server\n";
        return 1;
    }

    return server.waitForShutdown() ? 0 : 1;
}