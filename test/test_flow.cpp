#include "nrvna/flow.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
    std::filesystem::create_directories("./test_output");
    std::ofstream file("./test_output/job2.txt");
    file << "This is my test content2\n";
    file.close();
    nrvna::Flow flw("./test_output");
    auto result = flw.retrieve("job2");
    std::cout << "Job ID: " << result.jobId << std::endl;
    std::cout << "Content: " << result.inference << std::endl;
    return result.inference.empty() ? 1 : 0;
}