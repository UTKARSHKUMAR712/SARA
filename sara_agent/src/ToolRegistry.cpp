#include "../include/ToolRegistry.h"
#include "../include/WinAPIExecutor.h"
#include "../include/Logger.h"
#include "../include/plugins/mcp/MCPRegistry.h"

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
            // Check MCP Registry
            auto mcp_tools = MCPRegistry::instance().get_all_tools();
            bool is_mcp = false;
            for (auto& t : mcp_tools) {
                if (t["name"].get<std::string>() == name) {
                    is_mcp = true; break;
                }
            }
            if (is_mcp) {
                return MCPRegistry::instance().execute_tool(name, params);
            }
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
        
        if (!def.parameters_schema.is_null() && !def.parameters_schema.empty()) {
            tool["function"]["parameters"] = def.parameters_schema;
        } else {
            tool["function"]["parameters"] = {
                {"type", "object"},
                {"properties", json::object()},
                {"required", json::array()}
            };
        }
        defs.push_back(tool);
    }

    // Inject MCP Tools
    auto mcp_tools = MCPRegistry::instance().get_all_tools();
    for (auto& t : mcp_tools) {
        json tool;
        tool["type"] = "function";
        tool["function"]["name"] = t["name"];
        tool["function"]["description"] = t.value("description", "MCP Tool");
        tool["function"]["parameters"] = t.value("inputSchema", json::object());
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
            return fn(p);
        };
        reg.register_tool(std::move(def));
    };

    // Normalized tool result: {success, data, error} — consistent contract for Python
    auto norm = [](const ActionResult& r) -> json {
        json result;
        result["success"] = r.success;
        if (r.success) {
            json data = (r.data.is_null() || r.data.empty()) ? json::object() : r.data;
            if (!r.message.empty()) data["message"] = r.message;
            result["data"]  = data;
            result["error"] = nullptr;
        } else {
            result["data"]  = nullptr;
            result["error"] = r.message.empty() ? "Unknown error" : r.message;
        }
        return result;
    };

    wrap("play_youtube",      "Search and play on YouTube",      ToolCategory::media,          "LOW",    true,
        [&](const json& p){ return norm(ex.execute("play_youtube",       p)); });
    wrap("search_google",     "Google search in browser",        ToolCategory::browser,        "LOW",    true,
        [&](const json& p){ return norm(ex.execute("search_google",      p)); });
    wrap("open_website",      "Open URL in browser",             ToolCategory::browser,        "LOW",    true,
        [&](const json& p){ return norm(ex.execute("open_website",       p)); });
    wrap("open_app",          "Launch application",              ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("open_app",           p)); });
    wrap("close_process",     "Kill a running process",          ToolCategory::system,         "MED",    true,
        [&](const json& p){ return norm(ex.execute("close_process",      p)); });
    wrap("take_screenshot",   "Capture full screen",             ToolCategory::surveillance,   "LOW",    true,
        [&](const json& p){ return norm(ex.execute("take_screenshot",    p)); });
    wrap("capture_camera",    "Webcam photo",                    ToolCategory::surveillance,   "LOW",    true,
        [&](const json& p){ return norm(ex.execute("capture_camera",     p)); });
    wrap("get_system_stats",  "CPU/RAM/battery/GPU stats",       ToolCategory::monitoring,     "LOW",    true,
        [&](const json& p){ return norm(ex.execute("get_system_stats",   p)); });
    wrap("get_ip_address",    "Get local IP address",            ToolCategory::monitoring,     "LOW",    true,
        [&](const json& p){ return norm(ex.execute("get_ip_address",     p)); });
    wrap("volume_set",        "Set system volume",               ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("volume_set",         p)); });
    wrap("volume_mute",       "Mute/unmute audio",               ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("volume_mute",        p)); });
    wrap("change_brightness", "Change screen brightness",        ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("change_brightness",  p)); });
    wrap("notify",            "Windows toast notification",      ToolCategory::communication,  "LOW",    true,
        [&](const json& p){ return norm(ex.execute("notify",             p)); });
    wrap("clipboard_write",   "Copy text to clipboard",          ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("clipboard_write",    p)); });
    wrap("clipboard_read",    "Read clipboard text",             ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("clipboard_read",     p)); });
    wrap("focus_window",      "Focus a window by title",         ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("focus_window",       p)); });
    wrap("send_keys",         "Send keyboard input",             ToolCategory::system,         "MED",    true,
        [&](const json& p){ return norm(ex.execute("send_keys",          p)); });
    wrap("run_cmd",           "Run shell command (Desktop cwd)", ToolCategory::internal,       "HIGH",   true,
        [&](const json& p){ return norm(ex.execute("run_cmd",            p)); });
    wrap("run_powershell",    "Run PowerShell (Desktop cwd)",    ToolCategory::internal,       "HIGH",   true,
        [&](const json& p){ return norm(ex.execute("run_powershell",     p)); });
    wrap("write_file",        "Write file (path rel to Desktop)",ToolCategory::internal,       "LOW",    true,
        [&](const json& p){ return norm(ex.execute("write_file",         p)); });
    wrap("read_file",         "Read file (path rel to Desktop)", ToolCategory::internal,       "LOW",    true,
        [&](const json& p){ return norm(ex.execute("read_file",          p)); });
    wrap("list_dir",          "List directory (rel to Desktop)", ToolCategory::internal,       "LOW",    true,
        [&](const json& p){ return norm(ex.execute("list_dir",           p)); });
    wrap("task_complete",     "Signal task fully done",          ToolCategory::internal,       "LOW",    true,
        [&](const json& p){ return json{{"success",true},{"data",json{{"message","Task complete"}}},{"error",nullptr}}; });
    wrap("lock_pc",           "Lock system workstation",         ToolCategory::system,         "LOW",    true,
        [&](const json& p){ return norm(ex.execute("lock_pc",            p)); });

    Logger::instance().info("ToolRegistry: " + std::to_string(reg.list_all().size()) + " tools registered");
}

} // namespace sara
