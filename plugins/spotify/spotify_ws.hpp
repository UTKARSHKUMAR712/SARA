#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <winsock2.h>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

class SpotifyWS {
public:
    static SpotifyWS& instance();

    void start();   // starts server listener thread
    void stop();

    bool send_command(const json& cmd);
    bool is_client_connected() const;

private:
    SpotifyWS()  = default;
    ~SpotifyWS() = default;

    void server_loop();
    void client_loop(SOCKET client_sock);

    bool ws_handshake(SOCKET s);
    bool ws_send(SOCKET s, const std::string& payload);
    std::string ws_recv(SOCKET s);
    std::string base64_encode(const unsigned char* data, size_t len);
    std::string sha1_b64(const std::string& input);

    void process_frame(const std::string& payload);

    std::atomic<bool>   running_   {false};
    std::thread         server_thread_;
    std::thread         client_thread_;

    SOCKET              server_sock_ = INVALID_SOCKET;
    SOCKET              client_sock_ = INVALID_SOCKET;
    mutable std::mutex  send_mtx_;
    std::atomic<bool>   client_connected_ {false};
};

} // namespace sara
