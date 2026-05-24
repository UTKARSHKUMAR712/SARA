#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace sara {

using json = nlohmann::json;

class IPCClient {
public:
    IPCClient();
    ~IPCClient();

    bool connect(const std::string& pipe_name = "\\\\.\\pipe\\sara_agent");
    void disconnect();
    bool is_connected() const { return pipe_ != INVALID_HANDLE_VALUE; }

    json send_command(const std::string& action, const json& params = json::object());
    json send_message(const json& msg);
    json get_status();
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
};

}
