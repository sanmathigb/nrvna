#include "nrvna/server.hpp"
#include <iostream>
#include <filesystem>

int main() {
    // Your model is in ../models/ relative to build directory
    std::string model_path = "../models/tinyllama.gguf";

    // Check if model exists first
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "Model not found: " << model_path << std::endl;
        return 1;
    }

    nrvna::Server server(model_path, "./test_workspace");

    bool started = server.start();
    std::cout << "Server+Monitor started: " << (started ? "true" : "false") << std::endl;

    if (started) {
        std::cout << "Success! Real TinyLlama model loaded" << std::endl;
        server.stop();
    }

    return started ? 0 : 1;
}