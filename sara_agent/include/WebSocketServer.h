#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct WSClient {
    uint64_t id;
    std::string type; // "gui", "plugin"
    std::string name;
    uint64_t last_heartbeat;
};

using WSMessageHandler = std::function<void(uint64_t client_id, const std::string& type, const json& payload)>;
using WSConnectHandler = std::function<void(uint64_t client_id, const std::string& type)>;
using WSDisconnectHandler = std::function<void(uint64_t client_id)>;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    bool start(int port = 9080);
    void stop();
    bool is_running() const { return running_; }

    void set_message_handler(WSMessageHandler handler) { msg_handler_ = std::move(handler); }
    void set_connect_handler(WSConnectHandler handler) { conn_handler_ = std::move(handler); }
    void set_disconnect_handler(WSDisconnectHandler handler) { disc_handler_ = std::move(handler); }

    bool send(uint64_t client_id, const json& payload);
    bool broadcast(const json& payload, const std::string& type_filter = "");
    bool broadcast_to_type(const std::string& type, const json& payload);

    std::vector<WSClient> get_clients() const;
    int client_count() const;

private:
    void accept_loop();
    uint64_t next_id_ = 1;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    int port_ = 9080;

    WSMessageHandler msg_handler_;
    WSConnectHandler conn_handler_;
    WSDisconnectHandler disc_handler_;

    mutable std::mutex clients_mutex_;
    std::unordered_map<uint64_t, WSClient> clients_;
    std::unordered_map<uint64_t, std::thread> client_threads_;
    std::unordered_map<uint64_t, int> client_sockets_;

    static void handle_client(WebSocketServer* self, uint64_t cid, int sock);
    static bool ws_handshake(int sock, const std::string& request);
    static bool ws_send_frame(int sock, const std::string& data);
    static std::string ws_read_frame(int sock);
};

} // namespace sara
