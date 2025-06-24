#include "nrvna/server.hpp"
#include "nrvna/monitor.hpp"
#include <filesystem>
#include <iostream>
#include <csignal>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace nrvna {

    // =============================================================================
    // HttpReceiver class - handles HTTP requests from phone/browser
    // =============================================================================

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
            std::cout << "📱 Test URL: http://localhost:" << port_ << "/wrk" << std::endl;
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
        // URL decode helper function for multi-line content
        std::string urlDecode(const std::string& encoded) {
            std::string decoded;
            for (size_t i = 0; i < encoded.length(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.length()) {
                    // Decode %XX hex values
                    std::string hex = encoded.substr(i + 1, 2);
                    char ch = (char)std::stoi(hex, nullptr, 16);
                    decoded += ch;
                    i += 2;
                } else if (encoded[i] == '+') {
                    // Replace + with space
                    decoded += ' ';
                } else {
                    decoded += encoded[i];
                }
            }
            return decoded;
        }

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

            // DEBUG: Print the entire HTTP request
            std::cout << "=== HTTP REQUEST DEBUG ===" << std::endl;
            std::cout << "Bytes read: " << bytesRead << std::endl;
            std::cout << "Raw request:" << std::endl;
            std::cout << request << std::endl;
            std::cout << "=========================" << std::endl;

            if (request.find("POST /wrk") == 0) {
                // Extract content from POST request
                size_t bodyStart = request.find("\r\n\r\n");
                if (bodyStart != std::string::npos) {
                    std::string content = request.substr(bodyStart + 4);

                    // DEBUG: Show extracted content
                    std::cout << "=== CONTENT EXTRACTION ===" << std::endl;
                    std::cout << "Body start position: " << bodyStart << std::endl;
                    std::cout << "Total request length: " << request.length() << std::endl;
                    std::cout << "Content length: " << content.length() << std::endl;
                    std::cout << "Extracted content: '" << content << "'" << std::endl;
                    std::cout << "Content bytes: ";
                    for (char c : content) {
                        std::cout << (int)c << " ";
                    }
                    std::cout << std::endl;
                    std::cout << "=========================" << std::endl;

                    // Handle form-encoded content (case insensitive)
                    std::string contentLower = content;
                    std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

                    if (contentLower.find("content=") == 0) {
                        // Form submission: "content=Hello+1%0ALine+2" or "Content=Hello+1%0ALine+2"
                        content = content.substr(8); // Remove "content=" or "Content="

                        // URL decode the content (handles %0A newlines, + spaces, etc.)
                        content = urlDecode(content);

                        std::cout << "Form-decoded content: '" << content << "'" << std::endl;
                    }

                    // Remove any trailing newlines or null characters
                    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == '\0')) {
                        content.pop_back();
                    }

                    if (!content.empty()) {
                        // Generate simple job ID
                        auto now = std::chrono::system_clock::now();
                        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()).count();
                        std::string jobId = "http_" + std::to_string(timestamp);

                        // Follow work.cpp pattern - write to writing/ first, then atomic move
                        std::filesystem::create_directories(inputDir_ + "/writing");
                        std::filesystem::create_directories(inputDir_ + "/ready");

                        // Step 1: Write to writing/ directory (monitor ignores this)
                        std::string writingFile = inputDir_ + "/writing/" + jobId + ".txt";
                        std::ofstream file(writingFile);
                        if (file) {
                            file << content;
                            file.close();

                            // Step 2: Atomic move to ready/ directory (monitor scans this)
                            std::string readyFile = inputDir_ + "/ready/" + jobId + ".txt";
                            std::filesystem::rename(writingFile, readyFile);

                            std::cout << "✅ HTTP job created: " << jobId << std::endl;
                            std::cout << "📝 Content written: '" << content << "'" << std::endl;
                            std::cout << "🔄 Atomically moved to ready directory" << std::endl;

                            // Send success response
                            std::string response =
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 15\r\n"
                                "\r\n"
                                "Job queued!\n";
                            send(clientSocket, response.c_str(), response.length(), 0);
                        } else {
                            std::cout << "❌ Failed to write to writing file: " << writingFile << std::endl;
                            // Send error response
                            std::string response =
                                "HTTP/1.1 500 Internal Server Error\r\n"
                                "Content-Length: 12\r\n"
                                "\r\n"
                                "Write failed\n";
                            send(clientSocket, response.c_str(), response.length(), 0);
                        }
                    } else {
                        std::cout << "❌ No content extracted from HTTP request" << std::endl;
                        std::string response =
                            "HTTP/1.1 400 Bad Request\r\n"
                            "Content-Length: 15\r\n"
                            "\r\n"
                            "No content\n";
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
            }
        }

        int port_;
        std::string inputDir_;
        std::atomic<bool> running_;
        int serverSocket_;
        std::thread serverThread_;
    };

    // =============================================================================
    // Server class - your existing implementation (unchanged)
    // =============================================================================

    std::atomic<bool> Server::shutdown_requested_{false};

    Server::Server(const std::string& modelPath, const std::string& workspace)
        : model_path_(modelPath), workspace_(workspace) {
    }

    Server::~Server() {
        stop();
    }

    bool Server::start() {
        if (monitor_ || running_) {
            std::cerr << "Server already started" << std::endl;
            return false;
        }

        if (!setup()) return false;

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        monitor_ = std::make_unique<Monitor>(model_path_, workspace_);

        if (!monitor_->start()) {
            std::cerr << "Failed to start monitor" << std::endl;
            return false;
        }

        // Start HTTP server
        try {
            http_server_ = std::make_unique<HttpReceiver>(8080, workspace_ + "/input");
            http_server_->start();
            std::cout << "✅ HTTP server started - ready for phone connections!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ HTTP server failed to start: " << e.what() << std::endl;
            std::cerr << "✅ Monitor still running for local wrk/flw commands" << std::endl;
            // Continue without HTTP - local commands still work
        }

        running_ = true;
        return true;
    }

    void Server::stop() {
        if (running_) running_ = false;

        // Stop HTTP server
        if (http_server_) {
            http_server_->stop();
            http_server_.reset();
            std::cout << "HTTP server stopped" << std::endl;
        }

        if (monitor_) {
            monitor_->stop();
            monitor_.reset();
        }
    }

    bool Server::waitForShutdown() {
        if (!running_) return false;

        monitor_->run();
        while (running_ && !shutdown_requested_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        stop();
        return true;
    }

    bool Server::setup() {
        if (!std::filesystem::exists(model_path_)) {
            std::cerr << "Model file not found: " << model_path_ << std::endl;
            return false;
        }

        try {
            std::filesystem::create_directories(workspace_);
            std::filesystem::create_directories(workspace_ + "/input");
            std::filesystem::create_directories(workspace_ + "/input/ready");
            std::filesystem::create_directories(workspace_ + "/output");
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

    void Server::signalHandler(int signal) {
        shutdown_requested_ = true;
    }

} // namespace nrvna