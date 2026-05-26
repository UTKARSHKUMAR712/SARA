#include "plugins/mcp/MCPClient.h"
#include "Logger.h"
#include <chrono>

namespace sara {

MCPClient::MCPClient(std::shared_ptr<MCPTransport> transport) : transport_(transport) {
    transport_->set_on_message([this](const std::string& msg) {
        handle_message(msg);
    });
}

MCPClient::~MCPClient() {
    stop();
}

bool MCPClient::start(const std::string& cmd, const std::vector<std::string>& args) {
    return transport_->start(cmd, args);
}

void MCPClient::stop() {
    transport_->stop();
}

int MCPClient::get_next_id() {
    return request_id_++;
}

void MCPClient::handle_message(const std::string& msg) {
    try {
        json j = json::parse(msg);
        if (j.contains("id") && (j.contains("result") || j.contains("error"))) {
            int id = j["id"].get<int>();
            std::lock_guard<std::mutex> lock(promises_mutex_);
            auto it = pending_promises_.find(id);
            if (it != pending_promises_.end()) {
                it->second.set_value(j);
                pending_promises_.erase(it);
            }
        } else if (j.contains("method") && j["method"] == "notifications/message") {
            Logger::instance().info("MCP Notification: " + j.dump());
        }
    } catch (...) {
        Logger::instance().err("Failed to parse MCP message: " + msg);
    }
}

bool MCPClient::initialize() {
    int id = get_next_id();
    json req = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "initialize"},
        {"params", {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {}},
            {"clientInfo", {
                {"name", "SARA-MCP-Client"},
                {"version", "1.0.0"}
            }}
        }}
    };

    std::promise<json> p;
    auto future = p.get_future();
    {
        std::lock_guard<std::mutex> lock(promises_mutex_);
        pending_promises_[id] = std::move(p);
    }

    if (!transport_->send(req.dump())) return false;

    if (future.wait_for(std::chrono::seconds(60)) == std::future_status::ready) {
        json res = future.get();
        if (res.contains("result")) {
            // Send initialized notification
            json notif = {
                {"jsonrpc", "2.0"},
                {"method", "notifications/initialized"}
            };
            transport_->send(notif.dump());
            return true;
        }
    }
    return false;
}

std::vector<json> MCPClient::list_tools() {
    int id = get_next_id();
    json req = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "tools/list"}
    };

    std::promise<json> p;
    auto future = p.get_future();
    {
        std::lock_guard<std::mutex> lock(promises_mutex_);
        pending_promises_[id] = std::move(p);
    }

    transport_->send(req.dump());

    if (future.wait_for(std::chrono::seconds(60)) == std::future_status::ready) {
        json res = future.get();
        if (res.contains("result") && res["result"].contains("tools")) {
            return res["result"]["tools"].get<std::vector<json>>();
        }
    }
    return {};
}

json MCPClient::call_tool(const std::string& name, const json& arguments) {
    int id = get_next_id();
    json req = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "tools/call"},
        {"params", {
            {"name", name},
            {"arguments", arguments}
        }}
    };

    std::promise<json> p;
    auto future = p.get_future();
    {
        std::lock_guard<std::mutex> lock(promises_mutex_);
        pending_promises_[id] = std::move(p);
    }

    transport_->send(req.dump());

    if (future.wait_for(std::chrono::seconds(30)) == std::future_status::ready) {
        json res = future.get();
        if (res.contains("result") && res["result"].contains("content")) {
            return res["result"];
        } else if (res.contains("error")) {
            return {{"isError", true}, {"content", {{{"type", "text"}, {"text", res["error"].dump()}}}}};
        }
    }
    return {{"isError", true}, {"content", {{{"type", "text"}, {"text", "Tool call timed out or failed"}}}}};
}

} // namespace sara
