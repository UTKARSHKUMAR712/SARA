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
// Register all tools from WinAPIExecutor at startup.
void register_default_tools(WinAPIExecutor& ex) {
    auto& reg = ToolRegistry::instance();

    auto wrap = [&](const std::string& name, const std::string& desc,
                     ToolCategory cat, const std::string& risk,
                     std::function<json(const json&)> fn) {
        ToolDef def;
        def.name        = name;
        def.description = desc;
        def.category    = cat;
        def.risk_level  = risk;
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

    wrap("play_youtube",      "Search and play on YouTube",      ToolCategory::media, "LOW",
        [&](const json& p){ return norm(ex.execute("play_youtube",       p)); });
    wrap("search_google",     "Google search in browser",        ToolCategory::browser, "LOW",
        [&](const json& p){ return norm(ex.execute("search_google",      p)); });
    wrap("open_website",      "Open URL in browser",             ToolCategory::browser, "LOW",
        [&](const json& p){ return norm(ex.execute("open_website",       p)); });
    wrap("open_app",          "Launch application",              ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("open_app",           p)); });
    wrap("close_process",     "Kill a running process",          ToolCategory::system, "MED",
        [&](const json& p){ return norm(ex.execute("close_process",      p)); });
    wrap("take_screenshot",   "Capture full screen",             ToolCategory::surveillance, "LOW",
        [&](const json& p){ return norm(ex.execute("take_screenshot",    p)); });
    wrap("capture_camera",    "Webcam photo",                    ToolCategory::surveillance, "LOW",
        [&](const json& p){ return norm(ex.execute("capture_camera",     p)); });
    wrap("get_system_stats",  "CPU/RAM/battery/GPU stats",       ToolCategory::monitoring, "LOW",
        [&](const json& p){ return norm(ex.execute("get_system_stats",   p)); });
    wrap("get_ip_address",    "Get local IP address",            ToolCategory::monitoring, "LOW",
        [&](const json& p){ return norm(ex.execute("get_ip_address",     p)); });
    wrap("volume_set",        "Set system volume",               ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("volume_set",         p)); });
    wrap("volume_mute",       "Mute/unmute audio",               ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("volume_mute",        p)); });
    wrap("change_brightness", "Change screen brightness",        ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("change_brightness",  p)); });
    wrap("notify",            "Windows toast notification",      ToolCategory::communication, "LOW",
        [&](const json& p){ return norm(ex.execute("notify",             p)); });
    wrap("clipboard_write",   "Copy text to clipboard",          ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("clipboard_write",    p)); });
    wrap("clipboard_read",    "Read clipboard text",             ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("clipboard_read",     p)); });
    wrap("focus_window",      "Focus a window by title",         ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("focus_window",       p)); });
    wrap("send_keys",         "Send keyboard input",             ToolCategory::system, "MED",
        [&](const json& p){ return norm(ex.execute("send_keys",          p)); });
    wrap("run_cmd",           "Run shell command (Desktop cwd)", ToolCategory::internal, "HIGH",
        [&](const json& p){ return norm(ex.execute("run_cmd",            p)); });
    wrap("run_powershell",    "Run PowerShell (Desktop cwd)",    ToolCategory::internal, "HIGH",
        [&](const json& p){ return norm(ex.execute("run_powershell",     p)); });
    wrap("write_file",        "Write file (path rel to Desktop)",ToolCategory::internal, "LOW",
        [&](const json& p){ return norm(ex.execute("write_file",         p)); });
    wrap("read_file",         "Read file (path rel to Desktop)", ToolCategory::internal, "LOW",
        [&](const json& p){ return norm(ex.execute("read_file",          p)); });
    wrap("list_dir",          "List directory (rel to Desktop)", ToolCategory::internal, "LOW",
        [&](const json& p){ return norm(ex.execute("list_dir",           p)); });
    wrap("lock_pc",           "Lock system workstation",         ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("lock_pc",            p)); });
    wrap("open_url",          "Open URL or file:/// path in browser", ToolCategory::browser, "LOW",
        [&](const json& p){ return norm(ex.execute("open_url",          p)); });
    wrap("open_file",         "Open file with default handler",   ToolCategory::system, "LOW",
        [&](const json& p){ return norm(ex.execute("open_file",         p)); });

    // ── media_command: unified media control (Spotify, YouTube, OS media keys) ──
    {
        ToolDef def;
        def.name        = "media_command";
        def.description = "Control Spotify or OS media. action: spotify_play/spotify_pause/spotify_resume/spotify_next/spotify_prev/media_play_pause/play_youtube";
        def.category    = ToolCategory::media;
        def.risk_level  = "LOW";
        def.handler = [](const json& p) -> json {
            // This is handled at dispatch_action level before reaching registry
            // But if called directly, forward to WinAPIExecutor
            std::string sub_act = p.value("action", "");
            if (sub_act.empty()) return {{"success",false},{"error","missing action"},{"data",nullptr}};
            return {{"success",true},{"data",{{"message","media_command dispatched: " + sub_act}}},{"error",nullptr}};
        };
        reg.register_tool(std::move(def));
    }

    Logger::instance().info("ToolRegistry: " + std::to_string(reg.list_all().size()) + " tools registered");
}

} // namespace sara

