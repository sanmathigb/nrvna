#include "nrvna/http_receiver.hpp"
#include <iostream>
#include <csignal>

std::unique_ptr<nrvna::HttpReceiver> receiver;

void signal_handler(int) {
    if (receiver) receiver->stop();
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <workspace>\n";
        return 1;
    }

    signal(SIGINT, signal_handler);

    int port = std::stoi(argv[1]);
    receiver = std::make_unique<nrvna::HttpReceiver>(port, argv[2]);
    receiver->start();

    std::cout << "HTTP server running on http://0.0.0.0:" << port << "\n";
    std::cout << "POST images to /upload\n";

    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }

    return 0;
}