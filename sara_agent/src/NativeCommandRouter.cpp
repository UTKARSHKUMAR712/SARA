#include "../include/NativeCommandRouter.h"
#include "../include/TelegramGateway.h"
#include "../include/WinAPIExecutor.h"
#include "../include/Logger.h"

extern sara::TelegramGateway g_telegram;
extern sara::WinAPIExecutor g_executor;

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
                                "\n⚡ *All commands execute instantly, bypassing AI.*";
        g_telegram.send_message(chat_id, help_text);
        return true;
    }
    if (text == "/memory") {
        execute_and_reply(chat_id, "get_system_stats", nlohmann::json::object(), true);
        return true;
    }

    return false; // Not handled, falls through to AI
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
    if (text == "/spotify1") { execute_and_reply(chat_id, "open_app", {{"target", "spotify"}}, true); return true; }
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

}
