#include "../include/PluginManager.h"
#include "../include/Logger.h"
#include <windows.h>
#include <fstream>
#include <filesystem>

namespace sara {

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

PluginManager::~PluginManager() {
    shutdown_all();
}

bool PluginManager::load_plugin(const std::string& name, const std::string& dll_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (plugins_.count(name)) {
        log("Plugin already loaded: " + name);
        return false;
    }

    if (!std::filesystem::exists(dll_path)) {
        log("Plugin DLL not found: " + dll_path);
        return false;
    }

    HMODULE handle = LoadLibraryA(dll_path.c_str());
    if (!handle) {
        log("Failed to load plugin DLL: " + dll_path + " (error=" + std::to_string(GetLastError()) + ")");
        return false;
    }

    auto create_fn = (IPlugin*(*)())GetProcAddress(handle, "create_plugin");
    if (!create_fn) {
        FreeLibrary(handle);
        log("Plugin missing create_plugin export: " + name);
        return false;
    }

    IPlugin* instance = create_fn();
    if (!instance) {
        FreeLibrary(handle);
        log("Plugin create_plugin returned null: " + name);
        return false;
    }

    PluginInfo info = instance->get_info();
    LoadedPlugin lp;
    lp.name = info.name;
    lp.path = dll_path;
    lp.handle = handle;
    lp.instance = instance;
    lp.enabled = true;
    lp.tools = instance->get_tools();

    plugins_[name] = std::move(lp);

    // Register tools
    for (auto& tool : plugins_[name].tools) {
        tool_to_plugin_[tool.name] = name;
    }

    log("Plugin loaded: " + name + " v" + info.version
        + " (" + std::to_string(plugins_[name].tools.size()) + " tools)");
    return true;
}

bool PluginManager::unload_plugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    if (it == plugins_.end()) return false;

    // Remove tool registrations
    for (auto& tool : it->second.tools) {
        tool_to_plugin_.erase(tool.name);
    }

    it->second.instance->shutdown();
    delete it->second.instance;

    if (it->second.handle) {
        FreeLibrary((HMODULE)it->second.handle);
    }

    plugins_.erase(it);
    log("Plugin unloaded: " + name);
    return true;
}

bool PluginManager::reload_plugin(const std::string& name) {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plugins_.find(name);
        if (it == plugins_.end()) return false;
        path = it->second.path;
    }
    unload_plugin(name);
    return load_plugin(name, path);
}

bool PluginManager::enable_plugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    if (it == plugins_.end()) return false;
    it->second.enabled = true;
    return true;
}

bool PluginManager::disable_plugin(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = plugins_.find(name);
    if (it == plugins_.end()) return false;
    it->second.enabled = false;
    return true;
}

json PluginManager::execute_tool(const std::string& tool_name, const json& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tool_to_plugin_.find(tool_name);
    if (it == tool_to_plugin_.end()) {
        return {{"success", false}, {"error", "Tool not found in any plugin: " + tool_name}};
    }

    auto pit = plugins_.find(it->second);
    if (pit == plugins_.end() || !pit->second.enabled) {
        return {{"success", false}, {"error", "Plugin not available: " + it->second}};
    }

    try {
        return pit->second.instance->execute_tool(tool_name, params);
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", std::string("Plugin error: ") + e.what()}};
    }
}

bool PluginManager::has_tool(const std::string& tool_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tool_to_plugin_.count(tool_name) > 0;
}

std::vector<PluginTool> PluginManager::get_all_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PluginTool> all;
    for (auto& [name, plugin] : plugins_) {
        if (!plugin.enabled) continue;
        for (auto& tool : plugin.tools) {
            all.push_back(tool);
        }
    }
    return all;
}

std::vector<std::string> PluginManager::list_plugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (auto& [name, plugin] : plugins_) {
        names.push_back(name + (plugin.enabled ? " (enabled)" : " (disabled)"));
    }
    return names;
}

void PluginManager::load_all_from_config(const std::string& config_path) {
    try {
        std::ifstream f(config_path);
        if (!f.is_open()) {
            log("No plugin config found at: " + config_path);
            return;
        }
        json config = json::parse(f);

        if (config.contains("plugins") && config["plugins"].is_array()) {
            for (auto& entry : config["plugins"]) {
                std::string name = entry.value("name", "");
                std::string path = entry.value("path", "");
                if (!name.empty() && !path.empty()) {
                    load_plugin(name, path);
                }
            }
        }
    } catch (const std::exception& e) {
        log("Error loading plugin config: " + std::string(e.what()));
    }
}

void PluginManager::shutdown_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [name, plugin] : plugins_) {
        plugin.instance->shutdown();
        delete plugin.instance;
        if (plugin.handle) {
            FreeLibrary((HMODULE)plugin.handle);
        }
    }
    plugins_.clear();
    tool_to_plugin_.clear();
    log("All plugins shut down");
}

void PluginManager::log(const std::string& msg) {
    if (log_fn_) log_fn_(msg);
    else Logger::instance().info("[PluginManager] " + msg);
}

} // namespace sara
