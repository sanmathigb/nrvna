#include "nrvna/runner.hpp"
#include <iostream>

int main() {
    nrvna::Runner runner("../models/tinyllama.gguf");
    auto result = runner.run("write a blog about use of ai and ml in agriculture");
    std::cout << "Result: " << result << std::endl;
    std::cout << "Length: " << result.length() << " characters" << std::endl;

    return result.empty() ? 1 : 0;
}