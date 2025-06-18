#include "nrvna/monitor.hpp"
#include "nrvna/work.hpp"
#include "nrvna/flow.hpp"
#include <iostream>
#include <filesystem>

int main() {
    std::string workspace = "./test_monitor_workspace";
    std::filesystem::remove_all(workspace);

    // Create job using Work class
    nrvna::Work work(workspace + "/input");
    std::string jobId = work.submit("explain artificial intelligence briefly");

    // Start monitor
    nrvna::Monitor monitor("../models/tinyllama.gguf", workspace);
    if (!monitor.start()) return 1;

    // Process jobs (should find and process our job)
    int processed = monitor.process();

    // Check result exists in output
    nrvna::Flow flow(workspace + "/output");
    auto result = flow.retrieve(jobId);

    if (!result.inference.empty()) {
        std::cout << "\n=== AI RESULT ===" << std::endl;
        std::cout << result.inference << std::endl;
        std::cout << "=== END RESULT ===" << std::endl;
    } else {
        std::cout << "No result found!" << std::endl;
    }

    monitor.stop();
    //std::filesystem::remove_all(workspace);

    return (processed == 1 && !result.inference.empty()) ? 0 : 1;
}