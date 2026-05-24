#include "../include/IPCServer.h"
#include "../include/Logger.h"
#include <windows.h>
#include <sstream>
#include <chrono>
#include <vector>
#include <unordered_set>

namespace sara {

json IPCMessage::to_json() const {
    json j;
    j["source"] = source;
    j["destination"] = destination;
    j["type"] = type;
    j["request_id"] = request_id;
    j["payload"] = payload;
    return j;
}

IPCMessage IPCMessage::from_json(const json& j) {
    IPCMessage msg;
    msg.source = j.value("source", "");
    msg.destination = j.value("destination", "");
    msg.type = j.value("type", "");
    msg.request_id = j.value("request_id", "");
    if (j.contains("payload")) msg.payload = j["payload"];
    return msg;
}

IPCServer::IPCServer() = default;
IPCServer::~IPCServer() { stop(); }

bool IPCServer::start(const std::string& pipe_name) {
    if (running_) return true;
    pipe_name_ = pipe_name;
    running_ = true;
    listener_ = std::thread(&IPCServer::listen_loop, this);
    Logger::instance().info("IPC server started: " + pipe_name_);
    return true;
}

void IPCServer::stop() {
    running_ = false;
    if (listener_.joinable()) {
        listener_.join();
    }
    Logger::instance().info("IPC server stopped");
}

void IPCServer::listen_loop() {
    while (running_) {
        std::wstring wpipe(pipe_name_.begin(), pipe_name_.end());
        HANDLE pipe = CreateNamedPipeW(
            wpipe.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192, 8192, 0, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            Logger::instance().err("IPC CreateNamedPipe failed: " +
                std::to_string(GetLastError()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (!ConnectNamedPipe(pipe, nullptr)) {
            if (GetLastError() != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                continue;
            }
        }

        handle_client(pipe);
        CloseHandle(pipe);
    }
}

bool IPCServer::handle_client(HANDLE pipe) {
    char buf[8192];
    DWORD bytes_read;

    if (!ReadFile(pipe, buf, sizeof(buf) - 1, &bytes_read, nullptr)) {
        return false;
    }
    buf[bytes_read] = 0;

    try {
        json j = json::parse(std::string(buf, bytes_read));
        auto msg = IPCMessage::from_json(j);
        auto response = process_message(msg);

        std::string resp_str = response.dump();
        DWORD written;
        WriteFile(pipe, resp_str.c_str(), resp_str.size(), &written, nullptr);
    } catch (const std::exception& e) {
        Logger::instance().err("IPC message error: " + std::string(e.what()));
        json error = {{"type", "error"}, {"payload", {{"message", e.what()}}}};
        std::string err_str = error.dump();
        DWORD written;
        WriteFile(pipe, err_str.c_str(), err_str.size(), &written, nullptr);
    }
    return true;
}

json IPCServer::process_message(const IPCMessage& msg) {
    if (handler_) {
        try {
            auto result = handler_(msg.to_json());
            return result;
        } catch (const std::exception& e) {
            return {{"type", "error"},
                    {"request_id", msg.request_id},
                    {"payload", {{"error", e.what()}}}};
        }
    }
    return {{"type", "error"},
            {"request_id", msg.request_id},
            {"payload", {{"error", "no handler registered"}}}};
}

bool IPCServer::broadcast(const std::string& type, const json& payload) {
    IPCMessage msg;
    msg.source = "agent";
    msg.destination = "broadcast";
    msg.type = type;
    msg.payload = payload;
    msg.request_id = "broadcast";

    std::wstring wpipe(pipe_name_.begin(), pipe_name_.end());
    HANDLE pipe = CreateFileW(wpipe.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;

    std::string str = msg.to_json().dump();
    DWORD written;
    WriteFile(pipe, str.c_str(), str.size(), &written, nullptr);
    CloseHandle(pipe);
    return true;
}

}
