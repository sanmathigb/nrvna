#pragma once
#include <string>
#include <thread>
#include <atomic>

namespace nrvna {
    class HttpReceiver {
    public:
        HttpReceiver(int port, const std::string& workspace);
        ~HttpReceiver();
        void start();
        void stop();

    private:
        int port_;
        int server_fd_;
        std::string workspace_;
        std::atomic<bool> running_;
        std::thread server_thread_;

        void serve();
        void handle_request(int client_socket);
    };
}