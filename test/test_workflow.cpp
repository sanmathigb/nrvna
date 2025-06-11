#include <nrvna/work.hpp>
#include <nrvna/flow.hpp>
#include <iostream>
#include <string>

int main() {
    std::string sharedDir = "./test_shared";
    nrvna::Work work(sharedDir);
    nrvna::Flow flow(sharedDir);
    auto jobId1 = work.submit("Hello World1!");
    auto jobId2 = work.submit("Hello World2!");
    auto jobId3 = work.submit("Hello World3!");
    auto jobId4 = work.submit("Hello World4!");
    auto jobId5 = work.submit("Hello World5!");
    auto jobId6 = work.submit("Hello World6!");

    auto result = flow.retrieve();
    std::cout << "Job: " << result.jobId << "\n";
    std::cout << "Content: " << result.inference << "\n";
    return result.inference.empty() ? 1 : 0;
}