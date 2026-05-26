#pragma once
#include "MCPClient.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace sara {

class MCPRegistry {
public:
    static MCPRegistry& instance() {
        static MCPRegistry inst;
        return inst;
    }

    bool load_config(const std::string& path);
    void shutdown();

    std::vector<json> get_all_tools();
    json execute_tool(const std::string& name, const json& arguments);

private:
    MCPRegistry() = default;
    ~MCPRegistry() = default;

    struct ServerContext {
        std::shared_ptr<MCPClient> client;
        std::vector<json> tools;
    };

    std::unordered_map<std::string, ServerContext> servers_;
    std::unordered_map<std::string, std::string> tool_to_server_; // tool name -> server name
};

} // namespace sara
