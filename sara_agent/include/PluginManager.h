#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include "../../plugins/plugin_api.h"

namespace sara {

using json = nlohmann::json;

struct LoadedPlugin {
    std::string name;
    std::string path;
    void*       handle = nullptr;
    IPlugin*    instance = nullptr;
    bool        enabled = true;
    json        config;
    std::vector<PluginTool> tools;
};

class PluginManager {
public:
    static PluginManager& instance();

    bool load_plugin(const std::string& name, const std::string& dll_path);
    bool unload_plugin(const std::string& name);
    bool reload_plugin(const std::string& name);

    bool enable_plugin(const std::string& name);
    bool disable_plugin(const std::string& name);

    json execute_tool(const std::string& tool_name, const json& params);
    bool has_tool(const std::string& tool_name) const;

    std::vector<PluginTool> get_all_tools() const;
    std::vector<std::string> list_plugins() const;

    void load_all_from_config(const std::string& config_path);
    void shutdown_all();

    using LogFn = std::function<void(const std::string&)>;
    void set_logger(LogFn fn) { log_fn_ = std::move(fn); }

private:
    PluginManager() = default;
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, LoadedPlugin> plugins_;
    std::unordered_map<std::string, std::string> tool_to_plugin_; // tool name → plugin name
    LogFn log_fn_;

    void log(const std::string& msg);
};

} // namespace sara
