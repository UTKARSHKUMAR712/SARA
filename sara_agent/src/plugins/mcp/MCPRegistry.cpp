#include "plugins/mcp/MCPRegistry.h"
#include "plugins/mcp/MCPTransport.h"
#include "Logger.h"
#include <fstream>
#include <algorithm>

namespace sara {

bool MCPRegistry::load_config(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().warning("No mcp_servers.json found at " + path);
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

            std::vector<std::string> categories;
            if (config.contains("categories") && config["categories"].is_array()) {
                for (auto& cat : config["categories"]) {
                    categories.push_back(cat.get<std::string>());
                }
            }

            if (cmd.empty()) continue;

            connect_server(server_name, cmd, args, categories);
        }
        return true;
    } catch (const std::exception& e) {
        Logger::instance().err("Error loading mcp_servers.json: " + std::string(e.what()));
        return false;
    }
}

bool MCPRegistry::connect_server(const std::string& name, const std::string& command,
                                  const std::vector<std::string>& args,
                                  const std::vector<std::string>& categories) {
    if (servers_.count(name)) {
        log("MCP server already connected: " + name);
        return false;
    }

    auto transport = std::make_shared<MCPStdioTransport>();
    auto client = std::make_shared<MCPClient>(transport);

    if (!client->start(command, args)) {
        log("Failed to start MCP server: " + name);
        return false;
    }

    if (!client->initialize()) {
        log("Failed to initialize MCP server: " + name);
        client->stop();
        return false;
    }

    auto tools = client->list_tools();
    for (auto& t : tools) {
        std::string t_name = t["name"].get<std::string>();
        tool_to_server_[t_name] = name;
    }

    ServerContext ctx;
    ctx.client = client;
    ctx.tools = tools;
    ctx.categories = categories;
    ctx.command = command;
    ctx.args = args;
    servers_[name] = ctx;

    log("MCP server connected: " + name + " (" + std::to_string(tools.size())
        + " tools, " + std::to_string(categories.size()) + " categories)");
    return true;
}

bool MCPRegistry::disconnect_server(const std::string& name) {
    auto it = servers_.find(name);
    if (it == servers_.end()) return false;

    // Remove tool mappings
    for (auto& t : it->second.tools) {
        std::string t_name = t["name"].get<std::string>();
        tool_to_server_.erase(t_name);
    }

    it->second.client->stop();
    servers_.erase(it);
    log("MCP server disconnected: " + name);
    return true;
}

bool MCPRegistry::reconnect_server(const std::string& name) {
    auto it = servers_.find(name);
    if (it == servers_.end()) return false;

    auto cmd = it->second.command;
    auto args = it->second.args;
    auto cats = it->second.categories;

    disconnect_server(name);
    return connect_server(name, cmd, args, cats);
}

MCPRegistry::ServerStatus MCPRegistry::get_server_status(const std::string& name) const {
    auto it = servers_.find(name);
    if (it == servers_.end()) return {name, false, 0, "Not found", {}};

    ServerStatus status;
    status.name = name;
    status.connected = true;
    status.tool_count = (int)it->second.tools.size();
    status.categories = it->second.categories;
    return status;
}

std::vector<MCPRegistry::ServerStatus> MCPRegistry::list_servers() const {
    std::vector<ServerStatus> list;
    for (auto& [name, ctx] : servers_) {
        ServerStatus s;
        s.name = name;
        s.connected = true;
        s.tool_count = (int)ctx.tools.size();
        s.categories = ctx.categories;
        list.push_back(s);
    }
    return list;
}

std::vector<json> MCPRegistry::get_all_tools(const std::string& category) {
    std::vector<json> all_tools;
    for (auto& [name, ctx] : servers_) {
        // Filter by category if specified
        if (!category.empty()) {
            bool has_category = false;
            for (auto& c : ctx.categories) {
                if (c == category) { has_category = true; break; }
            }
            if (!has_category) continue;
        }
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

bool MCPRegistry::has_tool(const std::string& name) const {
    return tool_to_server_.count(name) > 0;
}

void MCPRegistry::shutdown() {
    for (auto& [name, ctx] : servers_) {
        ctx.client->stop();
    }
    servers_.clear();
    tool_to_server_.clear();
    log("All MCP servers shut down");
}

void MCPRegistry::log(const std::string& msg) {
    if (log_fn_) log_fn_(msg);
    else Logger::instance().info("[MCP] " + msg);
}

} // namespace sara
