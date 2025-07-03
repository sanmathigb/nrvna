#ifndef NRVNA_HTTP_RECEIVER_HPP
#define NRVNA_HTTP_RECEIVER_HPP

#include <atomic>
#include <string>
#include <thread>

namespace nrvna {

class HttpReceiver {
public:
    explicit HttpReceiver(int port, const std::string& inputDir);
    ~HttpReceiver();

    void start();
    void stop();

private:
    void serverLoop();
    void handleRequest(int clientSocket);

    int port_;
    std::string inputDir_;
    std::atomic<bool> running_;
    int serverSocket_;
    std::thread serverThread_;
};

} // namespace nrvna

#endif // NRVNA_HTTP_RECEIVER_HPP
