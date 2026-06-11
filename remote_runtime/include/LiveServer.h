#pragma once
#include <winsock2.h>
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

    bool start(const std::string& directory, int port = 5500, bool bind_any = false);
    void stop();
    bool is_running() const { return running_; }
    int port() const { return port_; }

    LiveServer() = default;
    ~LiveServer() { stop(); }
private:
    void accept_loop();
    void handle_client(SOCKET sock);

    std::string directory_;
    int port_ = 0;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    SOCKET server_sock_ = INVALID_SOCKET;
};

} // namespace sara::remote
