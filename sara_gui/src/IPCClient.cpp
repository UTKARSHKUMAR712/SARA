#include "../include/IPCClient.h"
#include <sstream>
#include <chrono>
#include <thread>

namespace sara {

IPCClient::IPCClient() = default;
IPCClient::~IPCClient() { disconnect(); }

bool IPCClient::connect(const std::string& pipe_name) {
    disconnect();
    std::wstring wpipe(pipe_name.begin(), pipe_name.end());
    for (int retry = 0; retry < 10; retry++) {
        pipe_ = CreateFileW(wpipe.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe_ != INVALID_HANDLE_VALUE) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

void IPCClient::disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

json IPCClient::send_command(const std::string& action, const json& params) {
    json msg;
    msg["type"] = "command";
    msg["request_id"] = std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    msg["payload"]["action"] = action;
    if (!params.is_null()) {
        for (auto& [key, val] : params.items()) {
            msg["payload"][key] = val;
        }
    }
    return send_message(msg);
}

json IPCClient::get_status() {
    json msg;
    msg["type"] = "status";
    msg["request_id"] = "status_req";
    msg["payload"] = json::object();
    return send_message(msg);
}

json IPCClient::send_message(const json& msg) {
    disconnect();
    if (!connect()) {
        return {{"type", "error"}, {"payload", {{"error", "Not connected"}}}};
    }

    std::string str = msg.dump();
    DWORD written;
    if (!WriteFile(pipe_, str.c_str(), str.size(), &written, nullptr)) {
        disconnect();
        return {{"type", "error"}, {"payload", {{"error", "Write failed"}}}};
    }

    char buf[16384];
    DWORD bytes_read;
    if (!ReadFile(pipe_, buf, sizeof(buf) - 1, &bytes_read, nullptr)) {
        disconnect();
        return {{"type", "error"}, {"payload", {{"error", "Read failed"}}}};
    }
    buf[bytes_read] = 0;

    try {
        return json::parse(std::string(buf, bytes_read));
    } catch (...) {
        return {{"type", "error"}, {"payload", {{"error", "Parse failed"}}}};
    }
}

}
