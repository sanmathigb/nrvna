#include "nrvna/work.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <workspace> <content> [--email <address>]\n";
        return 1;
    }

    std::string workspace = argv[1];
    std::string inputDir = workspace + "/input";
    std::string email = "";

    // Parse content and optional email
    std::string content;
    int contentStart = 2;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--email" && i + 1 < argc) {
            email = argv[++i];
        } else {
            if (!content.empty()) content += " ";
            content += arg;
        }
    }

    nrvna::Work work(inputDir);
    std::string jobId = work.submit(content, email);

    if (!jobId.empty()) {
        std::cout << jobId << std::endl;
        return 0;
    } else {
        return 1;
    }
}