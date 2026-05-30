#include "../include/NativeCommandRouter.h"
#include "../include/TelegramGateway.h"
#include "../include/WinAPIExecutor.h"
#include "../include/Logger.h"
#include "../include/ConfigManager.h"
#include "../include/plugins/mcp/MCPRegistry.h"
#include "../../plugins/spotify/spotify_plugin.hpp"
#include <sstream>
#include <filesystem>
#include <vector>
#include <thread>
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


extern sara::TelegramGateway g_telegram;
extern sara::WinAPIExecutor g_executor;
extern sara::ConfigManager g_config;

namespace sara {

void NativeCommandRouter::execute_and_reply(const std::string& chat_id, const std::string& action, const nlohmann::json& params, bool send_reply) {
    auto res = g_executor.execute(action, params);
    if (send_reply) {
        if (res.success) {
            if (res.data.contains("path")) {
                std::string path = res.data["path"].get<std::string>();
                if (action.find("screenshot") != std::string::npos || action.find("camera") != std::string::npos || action.find("photo") != std::string::npos) {
                    g_telegram.send_photo(chat_id, path, res.message);
                } else {
                    g_telegram.send_message(chat_id, res.message);
                }
            } else {
                if (action == "clipboard_read" && res.data.contains("text")) {
                    g_telegram.send_message(chat_id, "📋 **Clipboard:**\n" + res.data["text"].get<std::string>());
                    return;
                }
                std::string msg = res.message;
                if (!res.data.empty() && res.data.is_object()) {
                    std::string pretty_data;
                    for (auto& el : res.data.items()) {
                        std::string val = el.value().is_string() ? el.value().get<std::string>() : el.value().dump();
                        pretty_data += "• **" + el.key() + "**: " + val + "\n";
                    }
                    if (!pretty_data.empty()) {
                        msg += "\n\n" + pretty_data;
                    }
                }
                g_telegram.send_message(chat_id, msg);
            }
        } else {
            g_telegram.send_message(chat_id, "❌ Error: " + res.message);
        }
    }
}

bool NativeCommandRouter::handle(const std::string& chat_id, const std::string& text) {
    if (text.empty() || text[0] != '/') return false;

    if (handle_volume(chat_id, text)) return true;
    if (handle_brightness(chat_id, text)) return true;
    // Spotify plugin — /sp prefix, always before media so it intercepts first
    if (text.size() >= 3 && text.substr(0,3) == "/sp") {
        return SpotifyPlugin::instance().handle_command(chat_id, text);
    }
    if (handle_media(chat_id, text)) return true;
    if (handle_system(chat_id, text)) return true;
    if (handle_screenshot(chat_id, text)) return true;
    if (handle_camera(chat_id, text)) return true;
    if (handle_network(chat_id, text)) return true;
    if (handle_clipboard(chat_id, text)) return true;
    if (handle_monitoring(chat_id, text)) return true;
    if (handle_window(chat_id, text)) return true;
    if (handle_file(chat_id, text)) return true;
    if (handle_automation(chat_id, text)) return true;
    if (handle_hotkey(chat_id, text)) return true;
    if (handle_search(chat_id, text)) return true;
    if (handle_news(chat_id, text)) return true;

    // ── MCP Management Commands ─────────────────────────────────────────
    if (text.find("/mcp connect ") == 0) {
        std::string args = text.substr(13);
        auto space = args.find(' ');
        std::string name = (space != std::string::npos) ? args.substr(0, space) : args;
        std::string cmd = (space != std::string::npos) ? args.substr(space + 1) : "uvx";
        std::vector<std::string> cmd_args = {cmd};
        // Parse remaining as args
        if (space != std::string::npos) {
            cmd_args.clear();
            std::istringstream iss(args);
            std::string token;
            while (iss >> token) cmd_args.push_back(token);
        }
        bool ok = MCPRegistry::instance().connect_server(name, cmd_args[0],
            std::vector<std::string>(cmd_args.begin() + 1, cmd_args.end()));
        g_telegram.send_message(chat_id, ok ? "✅ MCP connected: " + name : "❌ MCP connect failed: " + name);
        return true;
    }
    if (text.find("/mcp disconnect ") == 0) {
        std::string name = text.substr(16);
        bool ok = MCPRegistry::instance().disconnect_server(name);
        g_telegram.send_message(chat_id, ok ? "✅ MCP disconnected: " + name : "❌ MCP not found: " + name);
        return true;
    }
    if (text == "/mcp list" || text == "/mcp status") {
        auto servers = MCPRegistry::instance().list_servers();
        if (servers.empty()) {
            g_telegram.send_message(chat_id, "No MCP servers connected.");
            return true;
        }
        std::string reply = "📡 MCP Servers (" + std::to_string(servers.size()) + "):\n";
        for (auto& s : servers) {
            reply += "• " + s.name + " (" + std::to_string(s.tool_count) + " tools, "
                   + (s.connected ? "✅" : "❌") + ")\n";
        }
        g_telegram.send_message(chat_id, reply);
        return true;
    }
    if (text.find("/mcp reconnect ") == 0) {
        std::string name = text.substr(15);
        bool ok = MCPRegistry::instance().reconnect_server(name);
        g_telegram.send_message(chat_id, ok ? "✅ MCP reconnected: " + name : "❌ MCP reconnect failed: " + name);
        return true;
    }

    
    // Apps last to prevent dynamic /app1 from overriding other commands
    if (handle_apps(chat_id, text)) return true;
    
    if (text.find("/help") == 0) {
        if (text == "/help media") {
            g_telegram.send_message(chat_id, "🎵 **Media Commands**\n/play, /pause, /next, /prev, /stop, /ff, /rew, /yt <query>, /spotify1, /spotify0");
            return true;
        }
        if (text == "/help system") {
            g_telegram.send_message(chat_id, "💻 **System Commands**\n/lock, /sleep, /restart, /shutdown, /logoff, /desktop, /monitoroff, /screensaver\n/cpu, /ram, /bat, /disk, /uptime, /idle");
            return true;
        }
        std::string help_text = "🤖 **SARA Final Native Command Protocol**\n\n"
                                "🔊 **Volume**: /vol[0-100], /mute, /unmute\n"
                                "🔆 **Brightness**: /bright[0-100]\n"
                                "📸 **Screenshot**: /ss0, /ss1, /ssw0, /ssm1\n"
                                "📷 **Camera**: /cam0, /cam1, /camv0, /camoff\n"
                                "💻 **System**: /lock, /sleep, /shutdown, /restart, /logoff, /desktop, /monitoroff, /screensaver\n"
                                "🔄 **Restart**: /sararestart (Restart SARA and Cloudflare completely)\n"
                                "📊 **Monitor**: /cpu, /ram, /gpu, /bat, /disk, /temp, /uptime, /idle\n"
                                "🌐 **Network**: /wifi0, /wifi1, /bt0, /bt1, /ip, /pubip, /net, /ping\n"
                                "🎵 **Media**: /play, /pause, /next, /prev, /stop, /ff, /rew, /yt <query>\n"
                                "🚀 **Apps**: /msedge1, /spotify1, /calc1, /<anyapp>1\n"
                                "❌ **Close Apps**: /msedge0, /spotify0, /calc0, /<anyapp>0\n"
                                "📋 **Clipboard**: /clipget, /clipclear, /clip text\n"
                                "🪟 **Window**: /focus <app>, /minall, /restoreall, /closeactive\n"
                                "📁 **Files**: /explorer, /downloads, /open <path>, /delete <path>, /mkdir <path>\n"
                                "⚙️ **Processes**: /proc, /proc0 (List & Kill)\n"
                                "🤖 **Automation**: /rules, /tasks, /ruleoff, /ruleon\n"
                                "⌨️ **Hotkeys**: /hotkeys, /hotkeyadd, /hotkeyremove\n"
                                "🖥️ **Terminal**: /terminal, /terminal admin, /terminals, /killterminal <id>\n"
                                "\n⚡ *All commands execute instantly.*";
        g_telegram.send_message(chat_id, help_text);
        return true;
    }
    return false; // Not handled
}

bool NativeCommandRouter::handle_volume(const std::string& chat_id, const std::string& text) {
    if (text.find("/vol") == 0) {
        try {
            int vol = std::stoi(text.substr(4));
            execute_and_reply(chat_id, "volume_set", {{"level", vol}}, true);
            return true;
        } catch (...) { return false; }
    }
    if (text == "/mute") {
        execute_and_reply(chat_id, "volume_mute", {{"mute", true}}, true);
        return true;
    }
    if (text == "/unmute") {
        execute_and_reply(chat_id, "volume_mute", {{"mute", false}}, true);
        return true;
    }
    return false;
}

bool NativeCommandRouter::handle_brightness(const std::string& chat_id, const std::string& text) {
    if (text.find("/bright") == 0) {
        try {
            int level = std::stoi(text.substr(7));
            execute_and_reply(chat_id, "change_brightness", {{"level", level}}, true);
            return true;
        } catch (...) { return false; }
    }
    return false;
}

bool NativeCommandRouter::handle_media(const std::string& chat_id, const std::string& text) {
    if (text == "/play" || text == "/pause") { execute_and_reply(chat_id, "media_play_pause", {}, true); return true; }
    if (text == "/next") { execute_and_reply(chat_id, "media_next", {}, true); return true; }
    if (text == "/prev") { execute_and_reply(chat_id, "media_prev", {}, true); return true; }
    if (text == "/stop") { execute_and_reply(chat_id, "media_stop", {}, true); return true; }
    if (text == "/ff" || text == "/rew") {
        // Media keys don't natively seek, we mapped seek forward/backward earlier in MediaDock via keybd_event. Wait, we mapped them by generating key events. Let's use the executor.
        // Actually, Windows doesn't have a universal seek forward media key.
        return true; 
    }
    if (text.find("/yt ") == 0) {
        execute_and_reply(chat_id, "play_youtube", {{"query", text.substr(4)}}, true);
        return true;
    }
    if (text == "/spotify" || text == "/spotify1") { execute_and_reply(chat_id, "open_app", {{"target", "spotify"}}, true); return true; }
    if (text == "/spotify0") { execute_and_reply(chat_id, "close_process", {{"target", "spotify"}}, false); return true; }
    return false;
}

bool NativeCommandRouter::handle_system(const std::string& chat_id, const std::string& text) {
    if (text == "/lock") { execute_and_reply(chat_id, "lock_pc", {}, true); return true; }
    if (text == "/sleep") { execute_and_reply(chat_id, "sleep_pc", {}, true); return true; }
    if (text == "/restart") { execute_and_reply(chat_id, "restart_pc", {}, true); return true; }
    if (text == "/shutdown") { execute_and_reply(chat_id, "shutdown_pc", {}, true); return true; }
    if (text == "/logoff") { execute_and_reply(chat_id, "logoff_pc", {}, true); return true; }
    if (text == "/desktop") { execute_and_reply(chat_id, "show_desktop", {}, true); return true; }
    if (text == "/monitoroff") { execute_and_reply(chat_id, "monitor_off", {}, true); return true; }
    if (text == "/screensaver") { execute_and_reply(chat_id, "start_screensaver", {}, true); return true; }
    return false;
}

bool NativeCommandRouter::handle_screenshot(const std::string& chat_id, const std::string& text) {
    if (text == "/ss0") { execute_and_reply(chat_id, "take_screenshot", nlohmann::json::object(), false); return true; }
    if (text == "/ss1") { execute_and_reply(chat_id, "take_screenshot", nlohmann::json::object(), true); return true; }
    if (text == "/ssw0" || text == "/ssm0") { execute_and_reply(chat_id, "take_screenshot", nlohmann::json::object(), false); return true; }
    if (text == "/ssw1" || text == "/ssm1") { execute_and_reply(chat_id, "take_screenshot", nlohmann::json::object(), true); return true; }
    return false;
}

bool NativeCommandRouter::handle_camera(const std::string& chat_id, const std::string& text) {
    if (text == "/cam0") { execute_and_reply(chat_id, "capture_camera", nlohmann::json::object(), false); return true; }
    if (text == "/cam1") { execute_and_reply(chat_id, "capture_camera", nlohmann::json::object(), true); return true; }
    if (text == "/camv0") { execute_and_reply(chat_id, "open_app", {{"target", "microsoft.windows.camera:"}}, true); return true; }
    if (text == "/camoff") { execute_and_reply(chat_id, "close_process", {{"target", "WindowsCamera.exe"}}, true); return true; }
    return false;
}

bool NativeCommandRouter::handle_apps(const std::string& chat_id, const std::string& text) {
    if (text.size() > 2) {
        char last = text.back();
        if (last == '0' || last == '1') {
            std::string appName = text.substr(1, text.size() - 2) + ".exe";
            if (last == '1') {
                execute_and_reply(chat_id, "open_app", {{"target", appName}}, true);
            } else {
                execute_and_reply(chat_id, "close_process", {{"target", appName}}, true);
            }
            return true;
        }
    }
    return false;
}

bool NativeCommandRouter::handle_network(const std::string& chat_id, const std::string& text) {
    if (text == "/wifi0") { execute_and_reply(chat_id, "run_powershell", {{"command", "Disable-NetAdapter -Name 'Wi-Fi' -Confirm:$false"}}, false); return true; }
    if (text == "/wifi1") { execute_and_reply(chat_id, "run_powershell", {{"command", "Enable-NetAdapter -Name 'Wi-Fi' -Confirm:$false"}}, false); return true; }
    if (text == "/bt0" || text == "/bt1") { return true; }
    if (text == "/ip") { execute_and_reply(chat_id, "get_ip_address", nlohmann::json::object(), true); return true; }
    if (text == "/pubip") { execute_and_reply(chat_id, "run_powershell", {{"command", "(Invoke-RestMethod ipinfo.io/ip).Trim()"}}, true); return true; }
    if (text == "/net") { execute_and_reply(chat_id, "run_powershell", {{"command", "Get-NetAdapter | Select Name, Status | ConvertTo-Json"}}, true); return true; }
    if (text == "/ping") { execute_and_reply(chat_id, "run_cmd", {{"command", "ping 8.8.8.8"}}, true); return true; }
    return false;
}

bool NativeCommandRouter::handle_clipboard(const std::string& chat_id, const std::string& text) {
    if (text == "/clipget") { execute_and_reply(chat_id, "clipboard_read", nlohmann::json::object(), true); return true; }
    if (text == "/clipclear") { execute_and_reply(chat_id, "clipboard_write", {{"text", ""}}, true); return true; }
    if (text.find("/clip ") == 0) { execute_and_reply(chat_id, "clipboard_write", {{"text", text.substr(6)}}, true); return true; }
    return false;
}

bool NativeCommandRouter::handle_monitoring(const std::string& chat_id, const std::string& text) {
    if (text == "/cpu" || text == "/ram" || text == "/gpu" || text == "/bat" || text == "/disk" || text == "/temp" || text == "/uptime" || text == "/idle") {
        execute_and_reply(chat_id, "get_system_stats", nlohmann::json::object(), true); return true;
    }
    if (text == "/proc" || text == "/proc0") {
        bool bg = (text == "/proc0");
        std::string ps = bg ? 
            "Get-Process | Where-Object {$_.MainWindowHandle -eq 0} | Sort-Object WS -Descending | Select-Object -First 10 | Select-Object Name, Id, @{Name='RAM';Expression={[math]::Round($_.WS / 1MB, 2)}} | ConvertTo-Json" :
            "Get-Process | Where-Object {$_.MainWindowHandle -ne 0} | Sort-Object WS -Descending | Select-Object -First 10 | Select-Object Name, Id, @{Name='RAM';Expression={[math]::Round($_.WS / 1MB, 2)}} | ConvertTo-Json";
        
        auto res = g_executor.execute("run_powershell", {{"command", ps}});
        if (res.success && res.data.contains("output")) {
            nlohmann::json kb = nlohmann::json::array();
            std::string msg = bg ? "⚙️ **Top 10 Background Processes**\n" : "🖥️ **Top 10 Foreground Processes**\n";
            try {
                nlohmann::json procs = nlohmann::json::parse(res.data["output"].get<std::string>());
                if (procs.is_array()) {
                    for (auto& p : procs) {
                        std::string name = p.value("Name", "Unknown");
                        int pid = p.value("Id", 0);
                        double ram = p.value("RAM", 0.0);
                        std::string content = p.value("content", "");
                        if (content.size() > 1500) content = content.substr(0, 1500) + "...";
                        msg += "• `" + name + "` (" + std::to_string(pid) + ") - " + std::to_string(ram).substr(0, std::to_string(ram).find('.') + 3) + " MB\n";
                        kb.push_back(nlohmann::json::array({{{"text", "💀 Kill " + name}, {"callback_data", "dock_mon:kill_" + std::to_string(pid)}}}));
                    }
                    g_telegram.send_inline_keyboard(chat_id, msg, kb);
                    return true;
                }
            } catch(...) {}
        }
        g_telegram.send_message(chat_id, "❌ Failed to retrieve process list.");
        return true;
    }
    return false;
}

bool NativeCommandRouter::handle_window(const std::string& chat_id, const std::string& text) {
    if (text.find("/focus ") == 0) { execute_and_reply(chat_id, "focus_window", {{"title", text.substr(7)}}, true); return true; }
    if (text == "/minall" || text == "/restoreall") { execute_and_reply(chat_id, "show_desktop", nlohmann::json::object(), false); return true; }
    if (text == "/closeactive") { execute_and_reply(chat_id, "close_active", nlohmann::json::object(), true); return true; }
    return false;
}

bool NativeCommandRouter::handle_file(const std::string& chat_id, const std::string& text) {
    if (text.find("/open ") == 0) { execute_and_reply(chat_id, "open_app", {{"target", text.substr(6)}}, true); return true; }
    if (text.find("/delete ") == 0) { execute_and_reply(chat_id, "run_cmd", {{"command", "del /Q /F \"" + text.substr(8) + "\""}}, true); return true; }
    if (text.find("/mkdir ") == 0) { execute_and_reply(chat_id, "run_cmd", {{"command", "mkdir \"" + text.substr(7) + "\""}}, true); return true; }
    if (text == "/explorer") { execute_and_reply(chat_id, "open_app", {{"target", "explorer.exe"}}, true); return true; }
    if (text == "/downloads") { execute_and_reply(chat_id, "open_app", {{"target", "shell:Downloads"}}, true); return true; }
    return false;
}

bool NativeCommandRouter::handle_automation(const std::string& chat_id, const std::string& text) {
    if (text == "/rulelist" || text == "/rules") { execute_and_reply(chat_id, "list_event_rules", nlohmann::json::object(), true); return true; }
    // Rule off/on not fully native yet without talking to EventAutomationEngine, which we can route later.
    return false;
}

bool NativeCommandRouter::handle_hotkey(const std::string& chat_id, const std::string& text) {
    if (text == "/hotkeys") { return true; }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Search Plugin integration
// ─────────────────────────────────────────────────────────────────────────────

static std::string run_search_plugin(const std::string& json_input) {
    char exe_path_buf[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH);
    std::string exe_dir(exe_path_buf);
    auto slash = exe_dir.find_last_of("\\/");
    if (slash != std::string::npos) exe_dir = exe_dir.substr(0, slash);

    std::string plugin_exe = exe_dir + "\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe))
        plugin_exe = exe_dir + "\\plugins\\search_plugin\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe))
        plugin_exe = "C:\\Users\\utkarsh_kumar\\Desktop\\sara\\plugins\\search_plugin\\search_plugin.exe";
    if (!std::filesystem::exists(plugin_exe))
        return "{\"success\":false,\"error\":\"search_plugin.exe not found\"}";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE h_out_read = NULL, h_out_write = NULL;
    if (!CreatePipe(&h_out_read, &h_out_write, &sa, 0)) return "{\"success\":false,\"error\":\"CreatePipe failed\"}";
    SetHandleInformation(h_out_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE h_in_read = NULL, h_in_write = NULL;
    if (!CreatePipe(&h_in_read, &h_in_write, &sa, 0)) {
        CloseHandle(h_out_read); CloseHandle(h_out_write);
        return "{\"success\":false,\"error\":\"CreatePipe failed\"}";
    }
    SetHandleInformation(h_in_write, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = h_out_write;
    si.hStdError  = h_out_write;
    si.hStdInput  = h_in_read;

    PROCESS_INFORMATION pi = {};
    std::string cmd_line = "\"" + plugin_exe + "\"";
    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    bool created = CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(h_out_write);
    CloseHandle(h_in_read);

    if (!created) {
        CloseHandle(h_out_read); CloseHandle(h_in_write);
        return "{\"success\":false,\"error\":\"CreateProcess failed\"}";
    }

    DWORD written = 0;
    WriteFile(h_in_write, json_input.c_str(), (DWORD)json_input.size(), &written, NULL);
    CloseHandle(h_in_write);

    std::string output;
    char buf[4096];
    DWORD bytes_read = 0;
    while (ReadFile(h_out_read, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, 30000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(h_out_read);
    return output.empty() ? "{\"success\":false,\"error\":\"No output\"}" : output;
}

bool NativeCommandRouter::handle_search(const std::string& chat_id, const std::string& text) {
    bool scrape = false;
    std::string query;

    if (text.find("/searchscrape ") == 0) {
        query = text.substr(14);
        scrape = true;
    } else if (text.find("/search ") == 0) {
        query = text.substr(8);
        scrape = false;
    } else if (text == "/search" || text == "/searchscrape") {
        g_telegram.send_message(chat_id, "🔍 Usage:\n/search <query> — Fast web search\n/searchscrape <query> — Deep search with page content");
        return true;
    } else {
        return false;
    }

    if (query.empty()) return true;

    std::string mode_label = scrape ? "🔍 *Deep Search + Scrape*" : "🔍 *Searching the web...*";
    g_telegram.send_message(chat_id, mode_label + "\n\nQuery: `" + query + "`\n\nPlease wait...");

    nlohmann::json req = {
        {"plugin", "search"},
        {"action", "search"},
        {"query",  query},
        {"scrape", scrape},
        {"max_urls", 5}
    };
    std::string json_arg = req.dump();

    std::thread([chat_id, json_arg, query, scrape]() {
        std::string raw = run_search_plugin(json_arg);
        nlohmann::json resp;
        try { resp = nlohmann::json::parse(raw); } catch (...) { return; }

        if (!resp.value("success", false)) {
            g_telegram.send_message(chat_id, "❌ Search failed: " + resp.value("error", "Unknown error"));
            return;
        }

        std::string msg;
        if (!scrape) {
            msg = "🔍 **Search Results** for: `" + query + "`\n\n";
            auto& results = resp["results"];
            if (results.is_array() && !results.empty()) {
                int idx = 1;
                for (auto& r : results) {
                    std::string title   = r.value("title",   "Untitled");
                    std::string url     = r.value("url",     "");
                    std::string snippet = r.value("snippet", "");
                    if (snippet.size() > 1000) snippet = snippet.substr(0, 1000) + "...";
                    msg += std::to_string(idx++) + ". **" + title + "**\n";
                    if (!snippet.empty()) msg += "   " + snippet + "\n";
                    msg += "   🔗 " + url + "\n\n";
                    if (msg.size() > 3800) break;
                }
            } else { msg += "_No results found._"; }
        } else {
            msg = "🔍 **Deep Search** for: `" + query + "`\n\n";
            auto& sources = resp["sources"];
            if (sources.is_array() && !sources.empty()) {
                int idx = 1;
                for (auto& s : sources) {
                    std::string title   = s.value("title",   "Untitled");
                    std::string url     = s.value("url",     "");
                    std::string content = s.value("content", "");
                    if (content.size() > 1500) content = content.substr(0, 1500) + "...";
                    msg += std::to_string(idx++) + ". **" + title + "**\n";
                    msg += "   🔗 " + url + "\n";
                    if (!content.empty()) msg += "   " + content + "\n\n";
                    if (msg.size() > 3800) break;
                }
            } else { msg += "_No scraped content found._"; }
        }
        g_telegram.send_message(chat_id, msg);
    }).detach();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// News Plugin integration
// ─────────────────────────────────────────────────────────────────────────────

std::string run_news_plugin(const std::string& json_input) {
    char exe_path_buf[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path_buf, MAX_PATH);
    std::string exe_dir(exe_path_buf);
    auto slash = exe_dir.find_last_of("\\/");
    if (slash != std::string::npos) exe_dir = exe_dir.substr(0, slash);

    std::string plugin_exe = exe_dir + "\\news_plugin.exe";
    if (!std::filesystem::exists(plugin_exe)) {
        plugin_exe = exe_dir + "\\plugins\\news_plugin\\news_plugin.exe";
    }
    if (!std::filesystem::exists(plugin_exe)) {
        plugin_exe = "C:\\Users\\utkarsh_kumar\\Desktop\\sara\\plugins\\news_plugin\\news_plugin.exe";
    }

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE h_out_read = NULL, h_out_write = NULL;
    if (!CreatePipe(&h_out_read, &h_out_write, &sa, 0)) return "{\"success\":false,\"error\":\"Pipe fail\"}";
    SetHandleInformation(h_out_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE h_in_read = NULL, h_in_write = NULL;
    if (!CreatePipe(&h_in_read, &h_in_write, &sa, 0)) {
        CloseHandle(h_out_read); CloseHandle(h_out_write);
        return "{\"success\":false,\"error\":\"Pipe fail\"}";
    }
    SetHandleInformation(h_in_write, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = h_out_write;
    si.hStdError  = h_out_write;
    si.hStdInput  = h_in_read;

    PROCESS_INFORMATION pi = {};
    std::string cmd_line = "\"" + plugin_exe + "\"";
    std::vector<char> cmd_buf(cmd_line.begin(), cmd_line.end());
    cmd_buf.push_back('\0');

    bool created = CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(h_out_write);
    CloseHandle(h_in_read);

    if (!created) {
        CloseHandle(h_out_read); CloseHandle(h_in_write);
        return "{\"success\":false,\"error\":\"Failed to start news plugin\"}";
    }

    DWORD written = 0;
    WriteFile(h_in_write, json_input.c_str(), (DWORD)json_input.size(), &written, NULL);
    CloseHandle(h_in_write);

    std::string output;
    char buf[4096];
    DWORD bytes_read = 0;
    while (ReadFile(h_out_read, buf, sizeof(buf) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buf[bytes_read] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(h_out_read);
    return output.empty() ? "{\"success\":false,\"error\":\"No output from news plugin\"}" : output;
}

bool NativeCommandRouter::handle_news(const std::string& chat_id, const std::string& text) {
    std::string category;
    if (text.size() > 6 && text.substr(0, 6) == "/news ") {
        category = text.substr(6);
    } else if (text == "/news") {
        category = "general";
    } else {
        return false;
    }

    std::string pretty_cat = category;
    pretty_cat[0] = toupper(pretty_cat[0]);
    g_telegram.send_message(chat_id, "📰 *Fetching Top News...*\n\nCategory: `" + pretty_cat + "`\n\nPlease wait...");

    nlohmann::json req = {
        {"plugin", "news"},
        {"action", "get_news"},
        {"category", category},
        {"max_items", 10}
    };
    std::string json_arg = req.dump();

    std::thread([chat_id, json_arg, pretty_cat]() {
        std::string raw = run_news_plugin(json_arg);
        nlohmann::json resp;
        try { resp = nlohmann::json::parse(raw); } catch (...) { return; }

        if (!resp.value("success", false)) {
            g_telegram.send_message(chat_id, "❌ News fetch failed");
            return;
        }

        std::string msg = "📰 **Top " + pretty_cat + " News**\n\n";
        auto& articles = resp["articles"];
        if (articles.is_array() && !articles.empty()) {
            int idx = 1;
            for (auto& a : articles) {
                std::string title   = a.value("title",   "Untitled");
                std::string url     = a.value("url",     "");
                std::string source  = a.value("source",  "Link");
                std::string desc    = a.value("description", "");
                
                msg += std::to_string(idx++) + ". **" + title + "**\n";
                if (!desc.empty() && desc != title) msg += "   " + desc + "\n";
                msg += "   🔗 [" + source + "](" + url + ")\n\n";
                
                if (msg.size() > 3800) break;
            }
        } else {
            msg += "_No news found._";
        }
        g_telegram.send_message(chat_id, msg);
    }).detach();
    return true;
}

}
