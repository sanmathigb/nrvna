#include "nrvna/flow.hpp"
#include <iostream>
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <workspace> [job_id]" << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  " << argv[0] << " data-text" << std::endl;
        std::cerr << "  " << argv[0] << " data-code job123" << std::endl;
        return 1;
    }

    std::string workspace = argv[1];
    std::string outputDir = workspace + "/output";

    nrvna::Flow flow(outputDir);

    if (argc > 2) {
        // Specific job ID provided
        auto result = flow.retrieve(argv[2]);
        std::cout << result.inference << std::endl;
        return result.inference.empty() ? 1 : 0;
    } else {
        // Get latest result
        auto result = flow.retrieve();
        std::cout << result.inference << std::endl;
        return result.inference.empty() ? 1 : 0;
    }
}