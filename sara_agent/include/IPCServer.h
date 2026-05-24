#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace sara {

using json = nlohmann::json;
using IPCMessageHandler = std::function<json(const json& request)>;

struct IPCMessage {
    std::string source;
    std::string destination;
    std::string type;    // command, response, event, status, error
    std::string request_id;
    json payload;

    json to_json() const;
    static IPCMessage from_json(const json& j);
};

class IPCServer {
public:
    IPCServer();
    ~IPCServer();

    bool start(const std::string& pipe_name = "\\\\.\\pipe\\sara_agent");
    void stop();
    bool is_running() const { return running_; }

    void set_handler(IPCMessageHandler handler) { handler_ = std::move(handler); }

    bool broadcast(const std::string& type, const json& payload);

private:
    void listen_loop();
    bool handle_client(HANDLE pipe);
    json process_message(const IPCMessage& msg);

    std::string pipe_name_;
    std::atomic<bool> running_{false};
    std::thread listener_;
    IPCMessageHandler handler_;
};

}
