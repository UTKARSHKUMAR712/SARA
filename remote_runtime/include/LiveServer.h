#pragma once
#include <string>
#include <thread>
#include <atomic>

namespace sara::remote {

class LiveServer {
public:
    static LiveServer& instance() {
        static LiveServer s;
        return s;
    }

    bool start(const std::string& directory, int port = 5500);
    void stop();
    bool is_running() const { return running_; }
    int port() const { return port_; }

private:
    LiveServer() = default;
    ~LiveServer() { stop(); }

    void accept_loop();
    void handle_client(int sock);

    std::string directory_;
    int port_ = 0;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    int server_sock_ = -1;
};

} // namespace sara::remote
