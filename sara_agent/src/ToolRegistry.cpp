#include "../include/ToolRegistry.h"
#include "../include/WinAPIExecutor.h"
#include "../include/Logger.h"

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry inst;
    return inst;
}

void ToolRegistry::register_tool(ToolDef def) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[def.name] = std::move(def);
    Logger::instance().info("ToolRegistry: registered " + def.name);
}

bool ToolRegistry::has_tool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.count(name) > 0;
}

json ToolRegistry::execute(const std::string& name, const json& params) {
    std::function<json(const json&)> handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tools_.find(name);
        if (it == tools_.end()) {
            return {{"success", false}, {"error", "Tool not found: " + name}};
        }
        handler = it->second.handler;
    }
    try {
        return handler(params);
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", std::string(e.what())}};
    }
}

std::vector<ToolDef> ToolRegistry::get_by_category(ToolCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolDef> out;
    for (auto& [n, d] : tools_) {
        if (d.category == cat) out.push_back(d);
    }
    return out;
}

std::vector<ToolDef> ToolRegistry::list_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ToolDef> out;
    out.reserve(tools_.size());
    for (auto& [n, d] : tools_) out.push_back(d);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build OpenAI-compatible function definitions for all AI-visible tools
json ToolRegistry::build_tool_definitions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    json defs = json::array();
    for (auto& [name, def] : tools_) {
        if (!def.ai_visible) continue;
        json tool;
        tool["type"] = "function";
        tool["function"]["name"] = name;
        tool["function"]["description"] = def.description + " [" + def.risk_level + "]";
        tool["function"]["parameters"] = {
            {"type", "object"},
            {"properties", json::object()},
            {"required", json::array()}
        };
        defs.push_back(tool);
    }
    return defs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Register all semantic tools from WinAPIExecutor at startup.
// Call this from main() after constructing WinAPIExecutor.
void register_default_tools(WinAPIExecutor& ex) {
    auto& reg = ToolRegistry::instance();

    auto wrap = [&](const std::string& name, const std::string& desc,
                     ToolCategory cat, const std::string& risk, bool visible,
                     std::function<json(const json&)> fn) {
        ToolDef def;
        def.name        = name;
        def.description = desc;
        def.category    = cat;
        def.risk_level  = risk;
        def.ai_visible  = visible;
        def.handler     = [fn](const json& p) -> json {
            auto r = fn(p);
            return r;
        };
        reg.register_tool(std::move(def));
    };

    wrap("play_youtube",      "Search and play on YouTube",      ToolCategory::media,          "LOW",    true,
        [&](const json& p){ auto r = ex.execute("play_youtube", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("search_google",     "Google search in browser",        ToolCategory::browser,        "LOW",    true,
        [&](const json& p){ auto r = ex.execute("search_google", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("open_website",      "Open URL in browser",             ToolCategory::browser,        "LOW",    true,
        [&](const json& p){ auto r = ex.execute("open_website", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("take_screenshot",   "Capture full screen",             ToolCategory::surveillance,   "LOW",    true,
        [&](const json& p){ auto r = ex.execute("take_screenshot", p);
            return json{{"success",r.success},{"message",r.message},{"data",r.data}}; });
    wrap("capture_camera",    "Webcam photo",                    ToolCategory::surveillance,   "LOW",    true,
        [&](const json& p){ auto r = ex.execute("capture_camera", p);
            return json{{"success",r.success},{"message",r.message},{"data",r.data}}; });
    wrap("get_system_stats",  "CPU/RAM/battery/GPU stats",       ToolCategory::monitoring,     "LOW",    true,
        [&](const json& p){ auto r = ex.execute("get_system_stats", p);
            return json{{"success",r.success},{"data",r.data}}; });
    wrap("volume_set",        "Set system volume",               ToolCategory::system,         "LOW",    true,
        [&](const json& p){ auto r = ex.execute("volume_set", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("change_brightness", "Change screen brightness",        ToolCategory::system,         "LOW",    true,
        [&](const json& p){ auto r = ex.execute("change_brightness", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("notify",            "Windows toast notification",      ToolCategory::communication,  "LOW",    true,
        [&](const json& p){ auto r = ex.execute("notify", p);
            return json{{"success",r.success},{"message",r.message}}; });
    wrap("run_cmd",           "Run shell command",               ToolCategory::internal,       "HIGH",   true,
        [&](const json& p){ auto r = ex.execute("run_cmd", p);
            return json{{"success",r.success},{"message",r.message},{"data",r.data}}; });
    wrap("run_powershell",    "Run PowerShell command",          ToolCategory::internal,       "HIGH",   true,
        [&](const json& p){ auto r = ex.execute("run_powershell", p);
            return json{{"success",r.success},{"message",r.message},{"data",r.data}}; });

    Logger::instance().info("ToolRegistry: " + std::to_string(reg.list_all().size()) + " tools registered");
}

} // namespace sara
