#include "nrvna/work.hpp"
#include <iostream>
#include <string>
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <workspace> <content>" << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  " << argv[0] << " data-text \"write a blog post\"" << std::endl;
        std::cerr << "  " << argv[0] << " data-code \"debug this function\"" << std::endl;
        return 1;
    }

    std::string workspace = argv[1];
    std::string inputDir = workspace + "/input";

    // Combine remaining arguments into content
    std::string content;
    for (int i = 2; i < argc; i++) {
        if (i > 2) content += " ";
        content += argv[i];
    }

    nrvna::Work work(inputDir);
    std::string jobId = work.submit(content);
    if (!jobId.empty()) {
        std::cout << jobId << std::endl;
        return 0;
    } else {
        return 1;
    }
}