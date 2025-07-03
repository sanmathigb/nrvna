// src/http_receiver.cpp
#include "nrvna/http_receiver.hpp"
#include "nrvna/logger.hpp"
#include "nrvna/work.hpp" // Needed for Work::submit

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

namespace nrvna {

// Helper function for URL decoding (moved outside class if not already)
std::string urlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '+') {
            decoded += ' ';
        } else if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int hex = std::stoi(encoded.substr(i + 1, 2), nullptr, 16);
            decoded += static_cast<char>(hex);
            i += 2;
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

HttpReceiver::HttpReceiver(int port, const std::string& inputDir)
    : port_(port), inputDir_(inputDir), running_(false), serverSocket_(-1) {
}

HttpReceiver::~HttpReceiver() {
    stop();
}

void HttpReceiver::start() {
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

    LOG_INFO("HTTP server listening on port " + std::to_string(port_));
    LOG_INFO("Phone URL: http://your-mac-ip:" + std::to_string(port_) + "/wrk");
}

void HttpReceiver::stop() {
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

void HttpReceiver::serverLoop() {
    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (running_) {
                LOG_ERROR("Accept failed");
            }
            continue;
        }

        handleRequest(clientSocket);
        close(clientSocket);
    }
}

void HttpReceiver::handleRequest(int clientSocket) {
    char buffer[4096] = {0};

    // Read complete HTTP request (headers + body)
    std::string request;
    int totalBytes = 0;

    while (true) {
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        request.append(buffer, bytesRead);
        totalBytes += bytesRead;

        // Check if we have complete headers
        size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            // Extract Content-Length
            size_t clPos = request.find("Content-Length: ");
            if (clPos != std::string::npos) {
                size_t clStart = clPos + 16;
                size_t clEnd = request.find("\r\n", clStart);
                int contentLength = std::stoi(request.substr(clStart, clEnd - clStart));

                // Check if we have complete body
                size_t bodyStart = headerEnd + 4;
                int bodyReceived = request.length() - bodyStart;

                if (bodyReceived >= contentLength) {
                    break; // Complete request received
                }
            } else {
                break; // No Content-Length, headers only
            }
        }

        // Prevent infinite loop
        if (totalBytes > 16384) break;
    }

    // DEBUG: Print the complete HTTP request
    LOG_DEBUG("=== COMPLETE REQUEST ===");
    LOG_DEBUG("Total bytes: " + std::to_string(request.length()));
    LOG_DEBUG(request);
    LOG_DEBUG("========================");

    if (request.find("POST /wrk") == 0) {
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart != std::string::npos) {
            bodyStart += 4;
            std::string content = request.substr(bodyStart);

            // Handle form encoding: "content=actual+text"
            if (content.find("content=") == 0) {
                content = content.substr(8); // Remove "content="
                content = urlDecode(content); // Handle URL encoding
            }

            // Remove trailing newlines or null characters
            while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == '\0')) {
                content.pop_back();
            }

            if (!content.empty()) {
                // Use the Work class to submit the job
                nrvna::Work work(inputDir_); // inputDir_ is the base for input/writing/ready
                std::string jobId = work.submit(content);

                if (!jobId.empty()) {
                    LOG_INFO("HTTP job created: " + jobId);
                    LOG_DEBUG("Content written: '" + content + "'");

                    // Send success response
                    std::string response = std::string("HTTP/1.1 200 OK\r\n") +
                        "Content-Type: text/plain\r\n" +
                        "Content-Length: 15\r\n" +
                        "\r\n" +
                        "Job queued!\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                } else {
                    LOG_ERROR("Failed to submit job via Work class.");
                    // Send error response
                    std::string response = std::string("HTTP/1.1 500 Internal Server Error\r\n") +
                        "Content-Length: 12\r\n" +
                        "\r\n" +
                        "Write failed\n";
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
            } else {
                LOG_WARN("No content extracted from HTTP request");
                std::string response = std::string("HTTP/1.1 400 Bad Request\r\n") +
                    "Content-Length: 15\r\n" +
                    "\r\n" +
                    "No content\n";
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }
    } else if (request.find("GET /wrk") == 0) {
        // Serve simple form
        std::string html = std::string("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "\r\n" +
            "<!DOCTYPE html>" +
            "<html><head><title>nrvna</title></head>" +
            "<body style='font-family: monospace; max-width: 600px; margin: 50px auto;'>" +
            "<h1>ai works. you flow.</h1>" +
            "<form method='POST'>" +
            "<textarea name='content' rows='10' cols='60' " +
            "placeholder='Enter content to process...' style='width: 100%; font-family: monospace;'></textarea><br><br>" +
            "<button type='submit' style='padding: 10px 20px; font-family: monospace;'>wrk the AI</button>" +
            "</form>" +
            "</body></html>";
        send(clientSocket, html.c_str(), html.length(), 0);
    }
}

} // namespace nrvna

// This function is called from server.cpp
void startHttpServer(const std::string& inputDir, int port) {
    static nrvna::HttpReceiver httpServer(port, inputDir);
    httpServer.start();
}
