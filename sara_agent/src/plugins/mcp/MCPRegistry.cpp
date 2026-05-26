#include "plugins/mcp/MCPRegistry.h"
#include "plugins/mcp/MCPTransport.h"
#include "Logger.h"
#include <fstream>

namespace sara {

bool MCPRegistry::load_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().warning("No mcp_servers.json found at " + path + ". Skipping MCP initialization.");
        return false;
    }

    try {
        json j;
        file >> j;

        if (!j.contains("mcpServers")) return true;

        for (auto& [server_name, config] : j["mcpServers"].items()) {
            bool enabled = config.value("enabled", false);
            if (!enabled) continue;

            std::string cmd = config.value("command", "");
            std::vector<std::string> args;
            if (config.contains("args") && config["args"].is_array()) {
                for (auto& arg : config["args"]) {
                    args.push_back(arg.get<std::string>());
                }
            }

            if (cmd.empty()) continue;

            auto transport = std::make_shared<MCPStdioTransport>();
            auto client = std::make_shared<MCPClient>(transport);

            if (!client->start(cmd, args)) {
                Logger::instance().err("Failed to start MCP server: " + server_name);
                continue;
            }

            if (!client->initialize()) {
                Logger::instance().err("Failed to initialize MCP server: " + server_name);
                client->stop();
                continue;
            }

            auto tools = client->list_tools();
            for (auto& t : tools) {
                std::string t_name = t["name"].get<std::string>();
                tool_to_server_[t_name] = server_name;
            }

            servers_[server_name] = { client, tools };
            Logger::instance().info("Loaded MCP server: " + server_name + " with " + std::to_string(tools.size()) + " tools.");
        }
        return true;
    } catch (const std::exception& e) {
        Logger::instance().err("Error loading mcp_servers.json: " + std::string(e.what()));
        return false;
    }
}

void MCPRegistry::shutdown() {
    for (auto& [name, ctx] : servers_) {
        ctx.client->stop();
    }
    servers_.clear();
    tool_to_server_.clear();
}

std::vector<json> MCPRegistry::get_all_tools() {
    std::vector<json> all_tools;
    for (auto& [name, ctx] : servers_) {
        for (auto& t : ctx.tools) {
            all_tools.push_back(t);
        }
    }
    return all_tools;
}

json MCPRegistry::execute_tool(const std::string& name, const json& arguments) {
    auto it = tool_to_server_.find(name);
    if (it == tool_to_server_.end()) {
        return {{"isError", true}, {"content", {{{"type", "text"}, {"text", "MCP tool not found: " + name}}}}};
    }

    auto& server_name = it->second;
    auto& ctx = servers_[server_name];
    
    return ctx.client->call_tool(name, arguments);
}

} // namespace sara
