#pragma once
#include "MCPClient.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace sara {

class MCPRegistry {
public:
    static MCPRegistry& instance() {
        static MCPRegistry inst;
        return inst;
    }

    bool load_config(const std::string& path);
    void shutdown();

    // Dynamic server management
    bool connect_server(const std::string& name, const std::string& command,
                        const std::vector<std::string>& args,
                        const std::vector<std::string>& categories = {});
    bool disconnect_server(const std::string& name);
    bool reconnect_server(const std::string& name);

    // Health checks
    struct ServerStatus {
        std::string name;
        bool connected = false;
        int tool_count = 0;
        std::string error;
        std::vector<std::string> categories;
    };
    ServerStatus get_server_status(const std::string& name) const;
    std::vector<ServerStatus> list_servers() const;

    // Tool routing with category filter
    std::vector<json> get_all_tools(const std::string& category = "");
    json execute_tool(const std::string& name, const json& arguments);

    bool has_tool(const std::string& name) const;

    using LogFn = std::function<void(const std::string&)>;
    void set_logger(LogFn fn) { log_fn_ = std::move(fn); }

private:
    MCPRegistry() = default;
    ~MCPRegistry() = default;

    struct ServerContext {
        std::shared_ptr<MCPClient> client;
        std::vector<json> tools;
        std::vector<std::string> categories;
        std::string command;
        std::vector<std::string> args;
    };

    std::unordered_map<std::string, ServerContext> servers_;
    std::unordered_map<std::string, std::string> tool_to_server_;
    LogFn log_fn_;

    void log(const std::string& msg);
};

} // namespace sara
