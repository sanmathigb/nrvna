// src/http_receiver.cpp
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace nrvna {

class HttpReceiver {
public:
    explicit HttpReceiver(int port, const std::string& inputDir)
        : port_(port), inputDir_(inputDir), running_(false), serverSocket_(-1) {
    }

    ~HttpReceiver() {
        stop();
    }

    void start() {
        if (running_) return;

        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        int opt = 1;
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        if (bind(serverSocket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            close(serverSocket_);
            throw std::runtime_error("Failed to bind to port " + std::to_string(port_));
        }

        if (listen(serverSocket_, 3) < 0) {
            close(serverSocket_);
            throw std::runtime_error("Failed to listen");
        }

        running_ = true;
        serverThread_ = std::thread(&HttpReceiver::serverLoop, this);

        std::cout << "HTTP server listening on port " << port_ << std::endl;
        std::cout << "Phone URL: http://your-mac-ip:" << port_ << "/wrk" << std::endl;
    }

    void stop() {
        if (!running_) return;

        running_ = false;
        if (serverSocket_ >= 0) {
            close(serverSocket_);
            serverSocket_ = -1;
        }

        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }

private:
    void serverLoop() {
        while (running_) {
            struct sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);

            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0) {
                if (running_) {
                    std::cerr << "Accept failed" << std::endl;
                }
                continue;
            }

            handleRequest(clientSocket);
            close(clientSocket);
        }
    }

    void handleRequest(int clientSocket) {
        char buffer[4096] = {0};
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) return;

        std::string request(buffer, bytesRead);

        if (request.find("POST /wrk") == 0) {
            // Extract content from POST request
            size_t bodyStart = request.find("\r\n\r\n");
            if (bodyStart != std::string::npos) {
                std::string content = request.substr(bodyStart + 4);

                // Generate simple job ID (no Work class needed)
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                std::string jobId = "http_" + std::to_string(timestamp);

                // Write directly to input directory (like wrk command does)
                std::string inputFile = inputDir_ + "/ready/" + jobId + ".txt";

                // Ensure ready directory exists
                std::filesystem::create_directories(inputDir_ + "/ready");

                std::ofstream file(inputFile);
                if (file) {
                    file << content;
                    file.close();

                    std::cout << "HTTP job created: " << jobId << std::endl;

                    // Send success response
                    std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 15\r\n"
                        "\r\n"
                        "Job queued!\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                } else {
                    // Send error response
                    std::string response =
                        "HTTP/1.1 500 Internal Server Error\r\n"
                        "Content-Length: 12\r\n"
                        "\r\n"
                        "Write failed\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
            }
        } else if (request.find("GET /wrk") == 0) {
            // Serve simple form
            std::string html =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "<!DOCTYPE html>"
                "<html><head><title>nrvna</title></head>"
                "<body style='font-family: monospace; max-width: 600px; margin: 50px auto;'>"
                "<h1>nrvna is. you wrk the ai. so you can flow.</h1>"
                "<form method='POST'>"
                "<textarea name='content' rows='10' cols='60' "
                "placeholder='Enter content to process...' style='width: 100%; font-family: monospace;'></textarea><br><br>"
                "<button type='submit' style='padding: 10px 20px; font-family: monospace;'>wrk the AI</button>"
                "</form>"
                "</body></html>";
            send(clientSocket, html.c_str(), html.length(), 0);
        } else {
            // 404 response
            std::string response =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Not Found\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        }
    }

    int port_;
    std::string inputDir_;
    std::atomic<bool> running_;
    int serverSocket_;
    std::thread serverThread_;
};

} // namespace nrvna

// Add this function to your existing server.cpp main()
void startHttpServer(const std::string& inputDir, int port = 8080) {
    static nrvna::HttpReceiver httpServer(port, inputDir);
    httpServer.start();
}