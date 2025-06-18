#include "nrvna/server.hpp"
#include "nrvna/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <model_path> <workspace> [-v]\n";
        return 1;
    }

    // Default: quiet (errors only)
    nrvna::LogLevel logLevel = nrvna::LogLevel::ERROR;

    // Check for -v flag
    if (argc > 3 && std::string(argv[3]) == "-v") {
        logLevel = nrvna::LogLevel::DEBUG;
    }

    nrvna::Logger::instance().setLevel(logLevel);

    nrvna::Server server(argv[1], argv[2]);
    if (!server.start()) return 1;
    return server.waitForShutdown() ? 0 : 1;
}