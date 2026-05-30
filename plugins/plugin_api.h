#pragma once
#include <string>
#include <nlohmann/json.hpp>

// ── Plugin API v1 ────────────────────────────────────────────────────────────
// All plugins must implement this interface.
// Plugins are compiled as DLLs and loaded dynamically by PluginManager.

namespace sara {

using json = nlohmann::json;

struct PluginInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    std::vector<std::string> capabilities;
    std::vector<std::string> dependencies;
};

struct PluginTool {
    std::string name;
    std::string description;
    json parameters_schema;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual PluginInfo get_info() const = 0;

    virtual bool initialize(const json& config) = 0;
    virtual void shutdown() = 0;

    virtual std::vector<PluginTool> get_tools() const = 0;
    virtual json execute_tool(const std::string& name, const json& params) = 0;

    virtual bool handle_command(const std::string& command, const json& params, json& result) = 0;

    virtual void on_event(const std::string& event_type, const json& data) = 0;
};

// ── Plugin entry point signature ─────────────────────────────────────────────
extern "C" __declspec(dllexport) sara::IPlugin* create_plugin();
extern "C" __declspec(dllexport) void destroy_plugin(sara::IPlugin* plugin);

} // namespace sara
