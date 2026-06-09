#include "../include/MessageHandlers.h"
#include "../include/ActionDispatcher.h"
#include "../include/Logger.h"
#include "../include/SecurityManager.h"
#include "../include/TelegramGateway.h"
#include "../include/Scheduler.h"
#include "../include/RuntimeState.h"
#include "../include/EventAutomation.h"
#include "../include/DockRouter.h"
#include "../include/NativeCommandRouter.h"
#include "../include/CommandMap.h"
#include "../include/ConfigManager.h"
#include "../include/WinAPIExecutor.h"
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <chrono>
// Remote terminal runtime
#include "../../remote_runtime/include/TerminalSessionManager.h"
#include "../../remote_runtime/include/TerminalHttpServer.h"
#include "../../remote_runtime/include/CloudflaredManager.h"
#include "../../remote_runtime/include/FileBrowserManager.h"

using namespace sara;
extern TelegramGateway        g_telegram;
extern Scheduler              g_scheduler;
extern RuntimeState&          g_runtime;
extern EventAutomationEngine  g_event_engine;
extern ConfigManager          g_config;
extern WinAPIExecutor         g_executor;

extern std::unordered_map<std::string, std::string> g_pending_outputs;
extern std::mutex g_pending_mutex;
extern std::string g_last_playlist;
extern std::mutex g_last_playlist_mutex;
extern int g_actual_terminal_port;

extern std::string resolve_path(const std::string& path);

namespace sara {


long long parse_delay_suffix(const std::string& original_text, std::string& clean_text) {
    clean_text = original_text;
    std::string text_lower = original_text;
    for (auto& c : text_lower) {
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
    }

    size_t pos = std::string::npos;
    bool is_in = false;
    
    pos = text_lower.rfind(" after ");
    if (pos == std::string::npos) {
        pos = text_lower.rfind(" in ");
        if (pos != std::string::npos) {
            is_in = true;
        }
    }

    if (pos == std::string::npos) {
        return 0;
    }

    std::string delay_part = text_lower.substr(pos + (is_in ? 4 : 7));
    while (!delay_part.empty() && delay_part[0] == ' ') delay_part.erase(0, 1);

    size_t num_end = 0;
    double val = 0;
    try {
        val = std::stod(delay_part, &num_end);
    } catch (...) {
        return 0; 
    }

    std::string unit_part = delay_part.substr(num_end);
    while (!unit_part.empty() && unit_part[0] == ' ') unit_part.erase(0, 1);
    while (!unit_part.empty() && (unit_part.back() == ' ' || unit_part.back() == '\r' || unit_part.back() == '\n')) {
        unit_part.pop_back();
    }

    double multiplier = 1.0; 
    if (unit_part == "ms" || unit_part == "msec" || unit_part == "msecs" || unit_part == "millisecond" || unit_part == "milliseconds") {
        multiplier = 0.001;
    } else if (unit_part == "s" || unit_part == "sec" || unit_part == "secs" || unit_part == "second" || unit_part == "seconds") {
        multiplier = 1.0;
    } else if (unit_part == "m" || unit_part == "min" || unit_part == "mins" || unit_part == "minute" || unit_part == "minutes") {
        multiplier = 60.0;
    } else if (unit_part == "h" || unit_part == "hr" || unit_part == "hrs" || unit_part == "hour" || unit_part == "hours") {
        multiplier = 3600.0;
    } else {
        if (unit_part.empty()) {
            multiplier = 1.0;
        } else {
            return 0; 
        }
    }

    clean_text = original_text.substr(0, pos);
    while (!clean_text.empty() && clean_text.back() == ' ') clean_text.pop_back();

    double calculated = val * multiplier;
    if (calculated > 0.0 && calculated < 1.0) calculated = 1.0; 
    return (long long)calculated;
}

void handle_telegram_message(const std::string& chat_id, std::string text, const nlohmann::json& raw_message) {
    long long user_id = raw_message.value("sara_user_id", 0LL);
    if (user_id == 0) {
        try { user_id = std::stoll(chat_id); } catch (...) {}
    }
    if (user_id == 0) {
        Logger::instance().warning("Failed to extract user_id, dropping message");
        return;
    }
    std::string from_name = raw_message.contains("from") ? raw_message["from"].value("first_name", "Unknown") : "Unknown";

    Logger::instance().info("Telegram: [" + std::to_string(user_id) + " | " + from_name + "] " + text);

    // --- Natural Language Alias Processing ---
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    // Trim extra spaces
    while (!lower_text.empty() && lower_text.back() == ' ') lower_text.pop_back();
    while (!lower_text.empty() && lower_text.front() == ' ') lower_text.erase(lower_text.begin());
    
    if (lower_text == "sara shutdown" || lower_text == "shutdown sara") {
        text = "/sarashutdown";
    } else if (lower_text == "sara shutdown pc" || lower_text == "sara shutdownmy pc" || lower_text == "sara shutdown my pc" || lower_text == "shutdown pc") {
        text = "/shutdown";
    } else if (lower_text == "sara open media dock" || lower_text == "sara open media" || lower_text == "open media dock") {
        text = "/media";
    }
    // ------------------------------------------

    if (!SecurityManager::instance().check_rate_limit(user_id)) return;

    if (!SecurityManager::instance().is_trusted_user(user_id)) {
        int msg_id = raw_message.value("message_id", 0);
        if (msg_id > 0) g_telegram.api_call("deleteMessage", {{"chat_id", chat_id}, {"message_id", msg_id}});
        std::string tg_username, tg_first_name, tg_last_name;
        if (raw_message.contains("from")) {
            tg_username   = raw_message["from"].value("username", "");
            tg_first_name = raw_message["from"].value("first_name", "");
            tg_last_name  = raw_message["from"].value("last_name", "");
        }
        if (SecurityManager::instance().authenticate_root(user_id, text,
                tg_username, tg_first_name, tg_last_name)) {
            g_telegram.send_message(chat_id, "🔓 Device authorized. Enter Session Password.");
        } else {
            if (SecurityManager::instance().trigger_root_auth_flow(user_id))
                g_telegram.send_message(chat_id, "🔒 Provide Root Password from host.");
            else
                g_telegram.send_message(chat_id, "🔒 Incorrect or already pending.");
        }
        return;
    }
    if (!SecurityManager::instance().is_session_authenticated(user_id)) {
        int msg_id = raw_message.value("message_id", 0);
        if (msg_id > 0) g_telegram.api_call("deleteMessage", {{"chat_id", chat_id}, {"message_id", msg_id}});
        if (SecurityManager::instance().authenticate_session(user_id, text)) {
            g_telegram.send_message(chat_id, "🔓 Session authenticated. Welcome to SARA v3.0.");
        } else {
            g_telegram.send_message(chat_id, "🔒 Session locked. Enter Session Password.");
        }
        return;
    }

    std::string clean_text;
    long long delay_seconds = parse_delay_suffix(text, clean_text);
    if (delay_seconds > 0) {
        Task task;
        task.action = "delayed_message";
        task.parameters = {{"text", clean_text}};
        task.source = "telegram";
        task.source_id = chat_id;
        task.status = "pending";
        
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        task.execute_at = now + delay_seconds;
        
        g_scheduler.add_task(task);
        
        std::string delay_str = std::to_string(delay_seconds) + " seconds";
        if (delay_seconds >= 3600) {
            delay_str = std::to_string(delay_seconds / 3600.0).substr(0, 4) + " hours";
        } else if (delay_seconds >= 60) {
            delay_str = std::to_string(delay_seconds / 60.0).substr(0, 4) + " minutes";
        }
        
        g_telegram.send_message(chat_id, "⏳ Scheduled \"" + clean_text + "\" to execute in " + delay_str + ".");
        return;
    }

    if (raw_message.contains("callback_data")) {
        std::string callback_data = raw_message["callback_data"].get<std::string>();
        std::string callback_query_id = raw_message["callback_query_id"].get<std::string>();
        int message_id = raw_message.value("message_id", 0);

        if (DockRouter::handle_callback(chat_id, callback_data, callback_query_id, message_id)) return;

        if (callback_data.rfind("read_more:", 0) == 0) {
            std::string cb_chat_id = callback_data.substr(10);
            std::string full;
            { std::lock_guard<std::mutex> lock(g_pending_mutex);
              auto it = g_pending_outputs.find(cb_chat_id);
              if (it != g_pending_outputs.end()) { full = it->second; g_pending_outputs.erase(it); } }
            if (!full.empty()) {
                g_telegram.send_chunked_log(chat_id, full, "📄 Full Output:");
            } else {
                g_telegram.answer_callback_query(callback_query_id, "Output expired — rerun the command", true);
            }
            return;
        }
    }

    if (!text.empty() && text[0] == '/') {
        if (text.rfind("/dock", 0) == 0 || text.rfind("/system", 0) == 0 || 
            text.rfind("/monitor", 0) == 0 || text.rfind("/tasks", 0) == 0 ||
            text.rfind("/rules", 0) == 0 || text.rfind("/media", 0) == 0) {
            if (DockRouter::handle_command(chat_id, text)) return;
        }
        if (NativeCommandRouter::handle(chat_id, text)) return;
    }

    if (text == "/start") {
        g_telegram.send_message(chat_id,
            "🤖 SARA — Native Runtime\n\n"
            "Plugins & Docks available.\n"
            "• /dock — open dashboard\n"
            "• /system — system controls\n"
            "• /monitor — live stats\n"
            "• /sp — Spotify control\n"
            "• /terminal — open terminal\n"
            "• /terminal admin — open admin terminal\n"
            "• /workspace — open codespaces IDE\n"
            "• /files — open file browser\n"
            "• /sararestart — restart SARA agent\n\n"
            "Type /help for commands.");
        return;
    }
    if (text == "/help") {
        g_telegram.send_message(chat_id,
            "📋 SARA Commands:\n"
            "/start       — Welcome\n"
            "/status      — Runtime status\n"
            "/tasks       — Scheduled tasks\n"
            "/rules       — Event automation rules\n"
            "/monitor     — Live system stats\n"
            "/screenshot  — Take screenshot\n"
            "/photo       — Webcam photo\n"
            "/ping        — Latency check\n"
            "/test        — Diagnostic\n"
            "/cmd         — Run cmd (e.g. /cmd ipconfig)\n"
            "/cmd help    — Windows CLI help\n"
            "/terminal    — Start browser terminal\n"
            "/terminal admin — Start admin browser terminal\n"
            "/terminals   — List active terminals\n"
            "/killterminal <id> — Stop a terminal session\n"
            "/workspace   — Open SARA Workspace IDE\n"
            "/files       — Open File Browser\n"
            "/sararestart — Restart SARA and Cloudflare completely\n"
            "/sarashutdown — Kill SARA completely (no restart)\n\n"
            "🗣️ **Conversational Commands**\n"
            "\"sara shutdown\" — Shuts down the SARA bot\n"
            "\"sara shutdown pc\" — Shuts down the host Windows PC\n"
            "\"sara open media dock\" — Opens the media controller\n\n"
            "🔍 **Web Search**\n"
            "/search <query>       — Search the web (fast)\n"
            "/searchscrape <query> — Deep search with page content\n\n"
            "💬 You can also just type: open spotify, my ram usage, etc.\n"
            "/dock        — Dashboard\n"
            "/system      — System dock\n"
            "/sp          — Spotify\n\n"
            "💬 Plugins and docks handle everything else.");
        return;
    }
    if (text == "/status") {
        g_telegram.send_message(chat_id, g_runtime.format_status());
        return;
    }
    if (text == "/ping") { g_telegram.send_message(chat_id, "🏓 pong"); return; }
    
    if (text == "/sararestart") {
        g_telegram.send_message(chat_id, "🔄 *Nuking and Restarting SARA...*\n\nTerminating SARA and Cloudflare, then rebooting.");
        g_telegram.stop();
        system("cmd.exe /c start \"\" \"c:\\Users\\utkarsh_kumar\\Desktop\\sara\\bot\\sara_restart.bat\"");
        exit(0);
    }

    if (text == "/sarashutdown") {
        g_telegram.send_message(chat_id, "🔴 *Shutting down SARA...*\n\nTerminating all processes.");
        g_telegram.stop();
        system("cmd.exe /c start \"\" \"c:\\Users\\utkarsh_kumar\\Desktop\\sara\\bot\\kill_sara.bat\"");
        exit(0);
    }

    // ── Terminal commands ──────────────────────────────────────────────────────
    auto is_terminal_cmd = [&](bool& admin_mode) -> bool {
        admin_mode = false;
        if (text == "/terminal" || text == "open terminal" || text == "terminal session") return true;
        if (text == "/terminal_admin" || text == "/terminal admin" || text == "/terminal ps" || text == "open admin terminal" ||
            text == "admin terminal" || text == "admin shell" || text == "powershell admin") {
            admin_mode = true; return true;
        }
        return false;
    };
    bool is_admin_terminal = false;
    if (is_terminal_cmd(is_admin_terminal)) {
        auto& cfg = g_config.get();
        std::string shell = cfg.terminal_shell;
        if (is_admin_terminal) shell = "powershell.exe -NoExit -Command \"Write-Host 'SARA Admin Shell' -ForegroundColor Red\"";
        auto result = sara::remote::TerminalSessionManager::instance().create_session(
            chat_id, shell,
            cfg.terminal_default_cols, cfg.terminal_default_rows,
            is_admin_terminal,
            cfg.terminal_expiry_minutes);
        if (!result.success) {
            g_telegram.send_message(chat_id, "❌ Failed to create terminal session: " + result.error);
            return;
        }

        std::string cf_url = sara::remote::CloudflaredManager::instance().tunnel_url();
        
        // If Cloudflare tunnel is dead, boot it up now!
        if (cf_url.empty() && cfg.cloudflare_mode != "disabled") {
            g_telegram.send_message(chat_id, "☁️ *Starting Cloudflare Tunnel...*\n\nPlease wait a few seconds for the secure connection to establish. We will automatically create your terminal session and send you the link when ready!");
            
            auto on_url = [chat_id, shell, result, is_admin_terminal, cfg](const std::string& url) {
                std::thread([chat_id, shell, result, is_admin_terminal, cfg, url]() {
                    // Wait for DNS propagation while verifying cloudflared is alive
                    for (int i = 0; i < 10; i++) {
                        Sleep(1000);
                        if (!sara::remote::CloudflaredManager::instance().is_tunnel_alive()) {
                            g_telegram.send_message(chat_id,
                                "❌ Cloudflare tunnel died during startup.\n"
                                "Try `/terminal` again in a few seconds.");
                            return;
                        }
                    }

                    std::string base_url = url;
                    if (base_url.back() == '/') base_url.pop_back();
                    std::string final_url = base_url + "/t/" + result.session_id + "?token=" + result.token;
                    
                    std::string icon = is_admin_terminal ? "🛡" : "🖥";
                    std::string label = is_admin_terminal ? "Admin Terminal" : "Terminal";
                    
                    g_telegram.send_message(chat_id,
                        icon + " *" + label + " Ready*\n\n"
                        "Session: `" + result.session_id + "`\n"
                        "Shell: `" + shell + "`\n"
                        "Expires: " + std::to_string(cfg.terminal_expiry_minutes) + " minutes\n"
                        "🟢 Secure HTTPS via Cloudflare\n\n"
                        "Open in browser:\n" + final_url);
                }).detach();
            };

            bool cf_ok = false;
            if (cfg.cloudflare_mode == "named" && !cfg.cloudflare_tunnel_name.empty()) {
                cf_ok = sara::remote::CloudflaredManager::instance().start_named_tunnel(cfg.cloudflare_tunnel_name, on_url);
            } else {
                cf_ok = sara::remote::CloudflaredManager::instance().start_quick_tunnel(g_actual_terminal_port, on_url);
            }

            if (!cf_ok) {
                g_telegram.send_message(chat_id, "❌ Failed to start Cloudflare tunnel. Ensure cloudflared.exe is installed.");
            }
            return;
        }

        std::string base_url;
        if (!cf_url.empty()) {
            base_url = cf_url;
            if (base_url.back() == '/') base_url.pop_back();
        } else {
            base_url = "http://" + cfg.terminal_host + ":" + std::to_string(g_actual_terminal_port);
        }

        std::string url = base_url + "/t/" + result.session_id + "?token=" + result.token;
        std::string icon = is_admin_terminal ? "🛡" : "🖥";
        std::string label = is_admin_terminal ? "Admin Terminal" : "Terminal";
        std::string cf_note = cf_url.empty()
            ? "🔴 Cloudflare tunnel not ready yet. Try again in a few seconds."
            : "🟢 Secure HTTPS via Cloudflare";
        g_telegram.send_message(chat_id,
            icon + " *" + label + " Ready*\n\n"
            "Session: `" + result.session_id + "`\n"
            "Shell: `" + shell + "`\n"
            "Expires: " + std::to_string(cfg.terminal_expiry_minutes) + " minutes\n"
            + cf_note + "\n\n"
            "Open in browser:\n" + url);
        return;
    }

    if (text.rfind("/killterminal", 0) == 0) {
        std::string sid = text.size() > 14 ? text.substr(14) : "";
        while (!sid.empty() && sid[0] == ' ') sid.erase(0, 1);
        if (sid.empty()) {
            g_telegram.send_message(chat_id, "Usage: /killterminal <session_id>");
            return;
        }
        sara::remote::TerminalSessionManager::instance().destroy_session(sid, chat_id);
        g_telegram.send_message(chat_id, "✅ Terminal session `" + sid + "` terminated.");
        return;
    }

    if (text == "/terminals") {
        int n = sara::remote::TerminalSessionManager::instance().session_count();
        g_telegram.send_message(chat_id,
            "🖥 Active terminal sessions: " + std::to_string(n) + "\n"
            "Use /killterminal <id> to destroy one.");
        return;
    }
    // ── End terminal commands ──────────────────────────────────────────────────

    // ── File Browser command ───────────────────────────────────────────────────
    if (text == "/files" || text == "open files" || text == "file browser" || text == "files") {
        auto& cfg = g_config.get();

        if (!cfg.filebrowser_enabled) {
            g_telegram.send_message(chat_id, "❌ File Browser is disabled in settings.");
            return;
        }

        // Lazy-start the file browser daemon
        if (!sara::remote::FileBrowserManager::instance().start()) {
            g_telegram.send_message(chat_id, "❌ Failed to start File Browser daemon. Check logs.");
            return;
        }

        // Issue a token-only auth session (no PTY spawned) for File Browser access.
        auto result = sara::remote::TerminalSessionManager::instance().issue_auth_token(
            chat_id,
            cfg.terminal_expiry_minutes);
        if (!result.success) {
            g_telegram.send_message(chat_id, "❌ Failed to create files session: " + result.error);
            return;
        }

        std::string cf_url = sara::remote::CloudflaredManager::instance().tunnel_url();

        if (cf_url.empty() && cfg.cloudflare_mode != "disabled") {
            g_telegram.send_message(chat_id, "☁️ *Starting Cloudflare Tunnel...*\n\nThe file browser link will be sent once the tunnel is ready!");

            auto on_url = [chat_id, result, cfg](const std::string& url) {
                std::thread([chat_id, result, cfg, url]() {
                    for (int i = 0; i < 10; i++) {
                        Sleep(1000);
                        if (!sara::remote::CloudflaredManager::instance().is_tunnel_alive()) {
                            g_telegram.send_message(chat_id,
                                "❌ Cloudflare tunnel died during startup.\n"
                                "Try `/files` again in a few seconds.");
                            return;
                        }
                    }
                    std::string base = url;
                    if (!base.empty() && base.back() == '/') base.pop_back();
                    std::string file_url = base + "/files?token=" + result.token;
                    g_telegram.send_message(chat_id,
                        "📁 *File Browser Ready*\n\n"
                        "Root: `" + cfg.filebrowser_root + "`\n"
                        "🟢 Secure HTTPS via Cloudflare\n\n"
                        "Open in browser:\n" + file_url);
                }).detach();
            };

            bool cf_ok = false;
            if (cfg.cloudflare_mode == "named" && !cfg.cloudflare_tunnel_name.empty()) {
                cf_ok = sara::remote::CloudflaredManager::instance().start_named_tunnel(cfg.cloudflare_tunnel_name, on_url);
            } else {
                cf_ok = sara::remote::CloudflaredManager::instance().start_quick_tunnel(g_actual_terminal_port, on_url);
            }
            if (!cf_ok) {
                g_telegram.send_message(chat_id, "❌ Failed to start Cloudflare tunnel.");
            }
            return;
        }

        std::string base_url;
        if (!cf_url.empty()) {
            base_url = cf_url;
            if (base_url.back() == '/') base_url.pop_back();
        } else {
            base_url = "http://" + cfg.terminal_host + ":" + std::to_string(g_actual_terminal_port);
        }

        std::string file_url = base_url + "/files?token=" + result.token;
        std::string cf_note = cf_url.empty()
            ? "🔴 Cloudflare not ready. URL is local-only."
            : "🟢 Secure HTTPS via Cloudflare";

        g_telegram.send_message(chat_id,
            "📁 *File Browser Ready*\n\n"
            "Root: `" + cfg.filebrowser_root + "`\n"
            + cf_note + "\n\n"
            "Open in browser:\n" + file_url);
        return;
    }
    
    if (text == "/filesshutdown") {
        sara::remote::FileBrowserManager::instance().stop();
        system("cmd.exe /c start \"\" \"c:\\Users\\utkarsh_kumar\\Desktop\\sara\\bot\\kill_filebrowser.bat\"");
        g_telegram.send_message(chat_id, "🛑 File Browser has been completely shut down.");
        return;
    }
    
    if (text == "/workspaceshutdown") {
        sara::remote::TerminalSessionManager::instance().destroy_sessions_by_type("workspace");
        g_telegram.send_message(chat_id, "🛑 Workspace sessions have been closed. (File Browser remains active)");
        return;
    }

    // ── End File Browser commands ──────────────────────────────────────────────

    // ── Workspace command ──────────────────────────────────────────────────────
    if (text == "/workspace" || text == "open workspace" || text == "ide") {
        auto& cfg = g_config.get();

        // Workspace relies on FileBrowser backend for APIs, so we ensure it is started
        if (!cfg.filebrowser_enabled) {
            g_telegram.send_message(chat_id, "❌ File Browser is disabled in settings. Workspace requires it for file APIs.");
            return;
        }

        if (!sara::remote::FileBrowserManager::instance().start()) {
            g_telegram.send_message(chat_id, "❌ Failed to start backend File Service. Check logs.");
            return;
        }

        // Issue auth token
        auto result = sara::remote::TerminalSessionManager::instance().issue_auth_token(
            chat_id,
            cfg.terminal_expiry_minutes, "workspace");
        if (!result.success) {
            g_telegram.send_message(chat_id, "❌ Failed to create workspace session: " + result.error);
            return;
        }

        std::string cf_url = sara::remote::CloudflaredManager::instance().tunnel_url();

        if (cf_url.empty() && cfg.cloudflare_mode != "disabled") {
            g_telegram.send_message(chat_id, "☁️ *Starting Cloudflare Tunnel...*\n\nThe Workspace link will be sent once the tunnel is ready!");

            auto on_url = [chat_id, result, cfg](const std::string& url) {
                std::thread([chat_id, result, cfg, url]() {
                    for (int i = 0; i < 10; i++) {
                        Sleep(1000);
                        if (!sara::remote::CloudflaredManager::instance().is_tunnel_alive()) {
                            g_telegram.send_message(chat_id,
                                "❌ Cloudflare tunnel died during startup.\n"
                                "Try `/workspace` again in a few seconds.");
                            return;
                        }
                    }
                    std::string base = url;
                    if (!base.empty() && base.back() == '/') base.pop_back();
                    std::string file_url = base + "/workspace/?token=" + result.token;
                    g_telegram.send_message(chat_id,
                        "💻 *SARA Workspace Ready*\n\n"
                        "🟢 Secure HTTPS via Cloudflare\n\n"
                        "Open in browser:\n" + file_url);
                }).detach();
            };

            bool cf_ok = false;
            if (cfg.cloudflare_mode == "named" && !cfg.cloudflare_tunnel_name.empty()) {
                cf_ok = sara::remote::CloudflaredManager::instance().start_named_tunnel(cfg.cloudflare_tunnel_name, on_url);
            } else {
                cf_ok = sara::remote::CloudflaredManager::instance().start_quick_tunnel(g_actual_terminal_port, on_url);
            }
            if (!cf_ok) {
                g_telegram.send_message(chat_id, "❌ Failed to start Cloudflare tunnel.");
            }
            return;
        }

        std::string base_url;
        if (!cf_url.empty()) {
            base_url = cf_url;
            if (base_url.back() == '/') base_url.pop_back();
        } else {
            base_url = "http://" + cfg.terminal_host + ":" + std::to_string(g_actual_terminal_port);
        }

        std::string file_url = base_url + "/workspace/?token=" + result.token;
        std::string cf_note = cf_url.empty()
            ? "🔴 Cloudflare not ready. URL is local-only."
            : "🟢 Secure HTTPS via Cloudflare";

        g_telegram.send_message(chat_id,
            "💻 *SARA Workspace Ready*\n\n"
            + cf_note + "\n\n"
            "Open in browser:\n" + file_url);
        return;
    }
    // ── End Workspace command ──────────────────────────────────────────────────
    if (text == "/monitor") {
        auto stats = g_runtime.get_summary();
        std::string msg = "📊 System Monitor\n"
            "CPU: " + std::to_string((int)stats["cpu_percent"].get<double>()) + "%\n"
            "RAM: " + std::to_string((int)stats["ram_percent"].get<double>()) + "%\n";
        if (stats["gpu_percent"].get<double>() >= 0)
            msg += "GPU: " + std::to_string((int)stats["gpu_percent"].get<double>()) + "%\n";
        if (stats["battery_percent"].get<int>() >= 0)
            msg += "Battery: " + std::to_string(stats["battery_percent"].get<int>()) + "%\n";
        g_telegram.send_message(chat_id, msg);
        return;
    }
    if (text == "/screenshot" || text == "/photo") {
        std::string action = (text == "/photo") ? "capture_camera" : "take_screenshot";
        g_telegram.send_action(chat_id, "upload_photo");
        auto res = dispatch_action(action, nlohmann::json::object());
        if (res.success && res.data.contains("path"))
            g_telegram.send_photo(chat_id, res.data["path"].get<std::string>(), "");
        else
            g_telegram.send_message(chat_id, res.message);
        return;
    }
    if (text == "/test") {
        g_telegram.send_message(chat_id, "🔬 Diagnostic...");
        auto stats = g_runtime.get_stats();
        std::string report;
        report += "Screenshot: " + std::string(dispatch_action("take_screenshot", {}).success ? "✅" : "❌") + "\n";
        report += "CPU: " + std::to_string((int)stats.cpu_percent) + "% "
                + "RAM: " + std::to_string((int)stats.ram_percent) + "%\n";
        report += "Uptime: " + std::to_string(stats.uptime_seconds / 3600) + "h\n";
        report += "WS: " + std::to_string(stats.ws_clients) + " clients";
        g_telegram.send_message(chat_id, report);
        return;
    }
    if (text.find("/cmd") == 0) {
        std::string rest = text.size() > 4 ? text.substr(4) : "";
        if (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);
        if (rest.find("help") == 0 || rest == "?" || rest == "-h") {
            static nlohmann::json help_data;
            if (help_data.is_null()) {
                std::ifstream f(resolve_path("settings\\cmd_help.json"));
                if (f.is_open()) { try { f >> help_data; } catch (...) {} }
            }
            if (help_data.is_null()) {
                g_telegram.send_message(chat_id, "❌ cmd_help.json not found or invalid.");
                return;
            }
            std::string topic = rest.size() > 4 ? rest.substr(4) : "";
            if (!topic.empty() && topic[0] == ' ') topic = topic.substr(1);

            if (topic.empty() || topic == "help" || topic == "?" || topic == "-h") {
                std::string msg = "💻 **Windows CLI Help**\n\n_Send `/cmd help <name>` to see commands._\n\n";
                for (auto& cat : help_data["categories"])
                    msg += cat["emoji"].get<std::string>() + " " + cat["name"].get<std::string>() + "\n";
                msg += "\n/`cmd help all` — Full list\n/`cmd <command>` — Run any command";
                g_telegram.send_message(chat_id, msg);
            } else if (topic == "all") {
                std::string msg;
                for (auto& cat : help_data["categories"]) {
                    msg += "\n" + cat["emoji"].get<std::string>() + " **" + cat["name"].get<std::string>() + "**\n";
                    for (auto& cmd : cat["commands"]) {
                        std::string line = "`" + cmd["cmd"].get<std::string>() + "` — " + cmd["desc"].get<std::string>() + "\n";
                        if (msg.size() + line.size() > 3500) { g_telegram.send_message(chat_id, msg); msg = line; }
                        else { msg += line; }
                    }
                }
                if (!msg.empty()) g_telegram.send_message(chat_id, msg);
            } else {
                std::string topic_lower = topic;
                for (auto& c : topic_lower) if (c >= 'A' && c <= 'Z') c += 32;
                size_t p;
                while ((p = topic_lower.find(" and ")) != std::string::npos) topic_lower.replace(p, 5, " & ");
                bool found = false;
                for (auto& cat : help_data["categories"]) {
                    std::string cat_name = cat["name"].get<std::string>();
                    std::string cat_lower = cat_name;
                    for (auto& c : cat_lower) if (c >= 'A' && c <= 'Z') c += 32;
                    if (cat_lower.find(topic_lower) != std::string::npos || topic_lower.find(cat_lower) != std::string::npos) {
                        std::string msg = cat["emoji"].get<std::string>() + " **" + cat_name + "**\n\n";
                        for (auto& cmd : cat["commands"]) {
                            std::string line = "`" + cmd["cmd"].get<std::string>() + "` — " + cmd["desc"].get<std::string>() + "\n";
                            if (msg.size() + line.size() > 3500) { g_telegram.send_message(chat_id, msg); msg = line; }
                            else { msg += line; }
                        }
                        if (!msg.empty()) g_telegram.send_message(chat_id, msg);
                        found = true; break;
                    }
                }
                if (!found)
                    g_telegram.send_message(chat_id, "❌ Category not found. Try `/cmd help` to see all categories.");
            }
            return;
        }
        if (!rest.empty()) {
            auto res = g_executor.execute("run_cmd", {{"command", rest}});
            if (res.success) {
                std::string out = res.data.value("output", "");
                if (out.size() > 3500) {
                    { std::lock_guard<std::mutex> lock(g_pending_mutex); g_pending_outputs[chat_id] = out; }
                    out = out.substr(0, 3500);
                    nlohmann::json btn = {{"text", "📄 Read Full Output"}, {"callback_data", "read_more:" + chat_id}};
                    nlohmann::json kb = {nlohmann::json::array({btn})};
                    if (!g_telegram.send_inline_keyboard(chat_id, "✅ Output (truncated):\n`" + out + "`", kb))
                        g_telegram.send_message(chat_id, "✅ Output (truncated):\n`" + out + "`");
                } else {
                    g_telegram.send_message(chat_id, "✅ Output:\n`" + out + "`");
                }
            } else
                g_telegram.send_message(chat_id, "❌ " + res.message);
        } else {
            g_telegram.send_message(chat_id, "Usage: /cmd <command>\nExample: /cmd ipconfig\n/cmd help — Show Windows CLI help");
        }
        return;
    }
    if (text == "/tasks") {
        auto tasks = g_scheduler.list_tasks();
        if (tasks.empty()) { g_telegram.send_message(chat_id, "No scheduled tasks."); return; }
        std::string reply = "📋 Tasks (" + std::to_string(tasks.size()) + "):\n";
        for (auto& t : tasks) {
            reply += "[" + t.id.substr(0, 8) + "] " + t.action + " (" + t.status + ")\n";
        }
        g_telegram.send_message(chat_id, reply);
        return;
    }
    if (text == "/rules") {
        auto rules = g_event_engine.list_rules();
        if (rules.empty()) { g_telegram.send_message(chat_id, "No event rules."); return; }
        std::string reply = "⚡ Rules (" + std::to_string(rules.size()) + "):\n";
        for (auto& r : rules) {
            reply += std::string(r.enabled ? "✅ " : "❌ ") + "[" + r.id.substr(0, 8) + "] " + r.description + "\n";
        }
        g_telegram.send_message(chat_id, reply);
        return;
    }

    {
        auto mr = CommandMap::instance().match(text);
        if (mr.entry) {
            auto& e = *mr.entry;
            nlohmann::json params = e.params;

            if (e.action == "play_music") {
                std::string query = mr.captured;
                if (query.empty()) query = params.value("query", "");
                if (query.empty()) { g_telegram.send_message(chat_id, "❌ What should I play?"); return; }
                std::string q_lower = query;
                for (auto& c : q_lower) if (c >= 'A' && c <= 'Z') c += 32;
                if (q_lower.size() >= 9 && q_lower.rfind(" playlist") == q_lower.size() - 9) {
                    std::string pname = query.substr(0, query.size() - 9);
                    { std::lock_guard<std::mutex> lock(g_last_playlist_mutex); g_last_playlist = pname; }
                    auto res = dispatch_action("spotify_playlist", {{"query", pname}});
                    g_telegram.send_message(chat_id, res.success ? "🎵 Playing playlist: " + pname : "❌ " + res.message);
                } else {
                    auto res = dispatch_action("spotify_play", {{"query", query}});
                    g_telegram.send_message(chat_id, res.success ? "🎵 Playing: " + query : "❌ " + res.message);
                }
                return;
            }
            if (e.action == "play_last_playlist") {
                std::string pname;
                { std::lock_guard<std::mutex> lock(g_last_playlist_mutex); pname = g_last_playlist; }
                if (pname.empty()) { g_telegram.send_message(chat_id, "❌ No playlist played yet."); return; }
                auto res = dispatch_action("spotify_playlist", {{"query", pname}});
                g_telegram.send_message(chat_id, res.success ? "🎵 Playing last playlist: " + pname : "❌ " + res.message);
                return;
            }
            if (e.action == "notify_text") {
                std::string msg = mr.captured;
                if (msg.empty()) { g_telegram.send_message(chat_id, "❌ Send: send notification <text>"); return; }
                auto res = g_executor.execute("notify", {{"title", "SARA"}, {"message", msg}});
                g_telegram.send_message(chat_id, res.success ? "✅ Notification sent" : "❌ " + res.message);
                return;
            }
            if (e.action == "open_url") {
                std::string url = mr.captured;
                if (url.empty()) { g_telegram.send_message(chat_id, "❌ Send: open website <url>"); return; }
                if (url.find("http") != 0) url = "https://" + url;
                auto res = g_executor.execute("open_website", {{"url", url}});
                g_telegram.send_message(chat_id, res.success ? "✅ Opening..." : "❌ " + res.message);
                return;
            }
            if (e.action == "notify") {
                auto res = g_executor.execute("notify", params);
                g_telegram.send_message(chat_id, res.success ? "✅ Notification sent" : "❌ " + res.message);
                return;
            }
            if (e.action == "type_text") {
                std::string keys = mr.captured;
                if (keys.empty()) { g_telegram.send_message(chat_id, "❌ Send: type this <text>"); return; }
                auto res = g_executor.execute("send_keys", {{"keys", keys}});
                g_telegram.send_message(chat_id, res.success ? "✅ Typing..." : "❌ " + res.message);
                return;
            }
            if (e.action == "focus_app") {
                std::string title = mr.captured;
                if (title.empty()) { g_telegram.send_message(chat_id, "❌ Send: focus window <name>"); return; }
                auto res = g_executor.execute("focus_window", {{"title", title}});
                g_telegram.send_message(chat_id, res.success ? "✅ Focused" : "❌ " + res.message);
                return;
            }
            if (e.action == "clipboard_copy") {
                std::string text_to = mr.captured;
                if (text_to.empty()) { g_telegram.send_message(chat_id, "❌ Send: copy this <text>"); return; }
                auto res = g_executor.execute("clipboard_write", {{"text", text_to}});
                g_telegram.send_message(chat_id, res.success ? "✅ Copied" : "❌ " + res.message);
                return;
            }
            if (e.action == "set_volume_level") {
                std::string cap = mr.captured;
                int level = 50;
                size_t start = cap.find_first_of("0123456789");
                if (start != std::string::npos) {
                    level = std::stoi(cap.substr(start));
                    if (level > 100) level = 100;
                }
                auto res = g_executor.execute("volume_set", {{"level", level}});
                g_telegram.send_message(chat_id, res.success ? "✅ Volume set to " + std::to_string(level) + "%" : "❌ " + res.message);
                return;
            }
            if (e.action == "set_brightness_level") {
                std::string cap = mr.captured;
                int level = 50;
                size_t start = cap.find_first_of("0123456789");
                if (start != std::string::npos) {
                    level = std::stoi(cap.substr(start));
                    if (level > 100) level = 100;
                }
                auto res = g_executor.execute("change_brightness", {{"level", level}});
                g_telegram.send_message(chat_id, res.success ? "✅ Brightness set to " + std::to_string(level) + "%" : "❌ " + res.message);
                return;
            }
            if (e.action == "open_app") {
                auto res = g_executor.execute("open_app", params);
                g_telegram.send_message(chat_id, res.success ? "✅ Opening " + params.value("name","") + "..." : "❌ " + res.message);
                return;
            }
            if (e.action == "run_cmd") {
                std::string cmd = params.value("command", "");
                auto res = g_executor.execute("run_cmd", {{"command", cmd}});
                if (res.success) {
                    std::string out = res.data.value("output", "");
                    if (out.size() > 3500) {
                        { std::lock_guard<std::mutex> lock(g_pending_mutex); g_pending_outputs[chat_id] = out; }
                        out = out.substr(0, 3500);
                        nlohmann::json btn = {{"text", "📄 Read Full Output"}, {"callback_data", "read_more:" + chat_id}};
                        nlohmann::json kb = {nlohmann::json::array({btn})};
                        if (!g_telegram.send_inline_keyboard(chat_id, "✅ Output (truncated):\n`" + out + "`", kb))
                            g_telegram.send_message(chat_id, "✅ Output (truncated):\n`" + out + "`");
                    } else {
                        g_telegram.send_message(chat_id, "✅ Output:\n`" + out + "`");
                    }
                } else
                    g_telegram.send_message(chat_id, "❌ " + res.message);
                return;
            }
            if (e.action == "search_google" || e.action == "search_plugin" || e.action == "search_plugin_scrape") {
                std::string q = params.value("query", mr.captured);
                if (q.empty()) q = mr.captured;
                if (q.empty()) {
                    g_telegram.send_message(chat_id, "🔍 What do you want me to search for?");
                    return;
                }
                // Route through search plugin — user requested ALL searches be deep search
                bool scrape = true;
                std::string cmd = "/searchscrape " + q;
                NativeCommandRouter::handle(chat_id, cmd);
                return;
            }
            if (e.action == "news_plugin") {
                std::string cat = params.value("category", "general");
                NativeCommandRouter::handle(chat_id, "/news " + cat);
                return;
            }
            dispatch_action(e.action, params);
            g_telegram.send_message(chat_id, "✅ Done");
            return;
        }
    }

    if (DockRouter::handle_command(chat_id, text)) return;
    g_telegram.send_message(chat_id,
        "🤖 Command not recognized. Try /help for available commands, "
        "/dock for dashboard, or /sp for Spotify.");
}

nlohmann::json handle_ipc_message(const nlohmann::json& msg) {
    auto type    = msg.value("type", "");
    auto req_id  = msg.value("request_id", "");
    auto payload = msg.value("payload", nlohmann::json::object());

    if (type == "command") {
        std::string action = payload.value("action", "");
        std::string target = payload.value("target", "");
        nlohmann::json params = payload.value("parameters", nlohmann::json::object());
        params["target"] = target;

        if (action == "reload_config") {
            auto& cfg = g_config.get();
            if (payload.contains("telegram")) {
                auto& t = payload["telegram"];
                cfg.telegram.token = t.value("token", cfg.telegram.token);
                cfg.telegram.password = t.value("password", cfg.telegram.password);
                cfg.telegram.polling_interval_ms = t.value("polling_interval_ms", cfg.telegram.polling_interval_ms);
                if (t.contains("allowed_user_ids") && t["allowed_user_ids"].is_array()) {
                    cfg.telegram.allowed_user_ids.clear();
                    for (auto& id : t["allowed_user_ids"])
                        cfg.telegram.allowed_user_ids.push_back(id.get<int64_t>());
                }
                g_telegram.stop();
                g_telegram.start(cfg.telegram.token, cfg.telegram.polling_interval_ms);
            }
            cfg.terminal_port = payload.value("terminal_port", cfg.terminal_port);
            cfg.terminal_shell = payload.value("terminal_shell", cfg.terminal_shell);
            cfg.terminal_expiry_minutes = payload.value("terminal_expiry_minutes", cfg.terminal_expiry_minutes);
            cfg.terminal_host = payload.value("terminal_host", cfg.terminal_host);
            cfg.cloudflare_enabled = payload.value("cloudflare_enabled", cfg.cloudflare_enabled);
            cfg.cloudflare_mode = payload.value("cloudflare_mode", cfg.cloudflare_mode);
            cfg.cloudflare_tunnel_name = payload.value("cloudflare_tunnel_name", cfg.cloudflare_tunnel_name);
            cfg.log_level = payload.value("log_level", cfg.log_level);
            g_config.save(resolve_path("settings.json"));
            return {{"type","response"}, {"request_id",req_id},
                    {"payload",{{"success",true},{"message","Config updated"}}}};
        }
        if (action == "get_root_password") {
            return {{"type","response"}, {"request_id",req_id},
                    {"payload",{{"success",true},{"password", SecurityManager::instance().get_latest_root_password()}}}};
        }
        if (action == "get_users") {
            nlohmann::json users_arr = nlohmann::json::array();
            for (auto& u : SecurityManager::instance().get_trusted_users()) {
                users_arr.push_back({{"user_id",u.user_id},{"username",u.username},
                    {"first_name",u.first_name},{"last_name",u.last_name},
                    {"phone",u.phone},{"added_at",u.added_at},{"blocked",u.blocked}});
            }
            return {{"type","response"}, {"request_id",req_id},
                    {"payload",{{"success",true},{"users",users_arr}}}};
        }
        if (action == "block_user") {
            bool ok = SecurityManager::instance().block_user(payload.value("user_id", 0LL));
            return {{"type","response"}, {"request_id",req_id},{"payload",{{"success",ok}}}};
        }
        if (action == "unblock_user") {
            bool ok = SecurityManager::instance().unblock_user(payload.value("user_id", 0LL));
            return {{"type","response"}, {"request_id",req_id},{"payload",{{"success",ok}}}};
        }
        if (action == "get_rules") {
            nlohmann::json rules_json = nlohmann::json::array();
            for (auto& r : g_event_engine.list_rules()) {
                rules_json.push_back({{"id",r.id},{"trigger_type",r.trigger_type},
                    {"trigger_value",r.trigger_value},{"enabled",r.enabled},
                    {"description",r.description}});
            }
            return {{"type","response"}, {"request_id",req_id},{"payload",{{"success",true},{"rules",rules_json}}}};
        }

        auto res = dispatch_action(action, params);
        return {{"type","response"}, {"request_id",req_id},
                {"payload",{{"success",res.success},{"message",res.message},{"data",res.data}}}};
    }

    if (type == "status") {
        return {{"type","status"}, {"request_id",req_id},
                {"payload",{{"running",true},
                            {"tasks",(int)g_scheduler.list_tasks().size()},
                            {"rules",(int)g_event_engine.list_rules().size()},
                            {"telegram",g_telegram.is_running()},
                            {"stats",g_runtime.get_summary()}}}};
    }
    if (type == "get_telegram_updates") {
        return {{"type","telegram_updates"}, {"request_id",req_id},
                {"payload",{{"messages",g_telegram.get_recent_messages(20)}}}};
    }
    return {{"type","error"}, {"request_id",req_id},{"payload",{{"error","unknown type"}}}};
}

} // namespace sara
