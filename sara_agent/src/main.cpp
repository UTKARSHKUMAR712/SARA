// SARA Agent v4.0 — Native Runtime
#include <winsock2.h>
#include <windows.h>
#include "../include/AgentInitializer.h"
#include "../include/Logger.h"
#include "../include/ConfigManager.h"
#include "../include/Scheduler.h"
#include "../include/SQLiteStore.h"
#include "../include/WinAPIExecutor.h"
#include "../include/TelegramGateway.h"
#include "../include/IPCServer.h"
#include "../include/plugins/mcp/MCPRegistry.h"
#include "../include/ActionValidator.h"
#include "../include/PermissionManager.h"
#include "../include/ProcessMonitor.h"
#include "../include/EventAutomation.h"
#include "../include/NetworkMonitor.h"
#include "../include/HotkeyManager.h"
#include "../include/SystemMonitor.h"
#include "../include/DockRouter.h"
#include "../include/SecurityManager.h"
#include "../include/NativeCommandRouter.h"
#include "../include/ToolRegistry.h"
#include "../include/CommandMap.h"
#include "../include/ActionDispatcher.h"
#include "../include/MessageHandlers.h"
// Remote Terminal Runtime
#include "../../remote_runtime/include/TerminalHttpServer.h"
#include "../../remote_runtime/include/TerminalSessionManager.h"
#include "../../remote_runtime/include/CloudflaredManager.h"
#include "../../remote_runtime/include/FileBrowserManager.h"
#include <fstream>
namespace sara { extern void register_default_tools(WinAPIExecutor& ex); }
#include "../include/WebSocketServer.h"
#include "../include/RuntimeState.h"
#include "../../plugins/spotify/spotify_plugin.hpp"
#include "../../plugins/spotify/spotify_commands.hpp"
#include "../../plugins/spotify/spotify_dock.hpp"
#include "../../plugins/spotify/spotify_ws.hpp"
#include "../../plugins/spotify/spotify_state.hpp"
#include <windows.h>
#include <csignal>
#include <atomic>
#include <sstream>
#include <chrono>
#include <thread>
#include <set>
#include <unordered_map>
#include <mutex>

using namespace sara;

static std::atomic<bool> g_running{true};
static std::string       g_exe_dir;

std::string resolve_path(const std::string& path) {
    if (path.empty() || path[0] == '/' || path[0] == '\\'
        || (path.size() > 2 && path[1] == ':'))
        return path;
    return g_exe_dir + path;
}

// ── Global subsystems ────────────────────────────────────────────────────────
ConfigManager          g_config;
Scheduler              g_scheduler;
SQLiteStore            g_store;
WinAPIExecutor         g_executor;
TelegramGateway        g_telegram;
IPCServer              g_ipc;
ProcessMonitor         g_proc_monitor;
EventAutomationEngine  g_event_engine;
NetworkMonitor         g_net_monitor;

WebSocketServer         g_ws_server;
RuntimeState&           g_runtime      = RuntimeState::instance();

// Actual port the terminal HTTP server ended up using (may differ from config after port rotation)
int g_actual_terminal_port = 0;

static void signal_handler(int) { g_running = false; }

extern std::unordered_map<std::string, std::string> g_pending_outputs;
extern std::mutex g_pending_mutex;

// ─────────────────────────────────────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    // --- Use AgentInitializer for lifecycle and path resolution ---
    auto& init = AgentInitializer::instance();
    init.initialize(argc, argv);

    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    g_exe_dir = buf;
    auto pos = g_exe_dir.find_last_of("\\/");
    if (pos != std::string::npos) g_exe_dir = g_exe_dir.substr(0, pos + 1);
    auto parent_pos = g_exe_dir.find_last_of("\\/", g_exe_dir.size() - 2);
    if (parent_pos != std::string::npos) {
        std::string parent = g_exe_dir.substr(0, parent_pos + 1);
        DWORD attr = GetFileAttributesA((parent + "data").c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            g_exe_dir = parent;
    }

    Logger::instance().init(resolve_path("logs"), LogLevel::INFO);
    Logger::instance().info("=== SARA v4.0 starting ===");

    // Mark runtime startup
    g_runtime.mark_startup();

    if (!g_config.load(resolve_path("settings.json")))
        Logger::instance().warning("No settings.json — using defaults");

    auto commands_dir = resolve_path("settings\\commands");
    if (GetFileAttributesA(commands_dir.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(commands_dir.c_str(), nullptr);
        Logger::instance().warning("Created settings/commands/ directory");
    }
    if (!CommandMap::instance().load_directory(commands_dir))
        Logger::instance().warning("No command files in settings/commands/ — natural language matching disabled");

    auto& cfg = g_config.get();

    // Initialize security
    SecurityManager::instance().initialize(resolve_path(cfg.data_dir));

    // Initialize database
    if (!g_store.open(resolve_path(cfg.data_dir) + "\\scheduler.db")) {
        Logger::instance().err("Failed to open database");
        return 1;
    }

    // Register default tools
    register_default_tools(g_executor);

    // Start scheduler
    g_scheduler.set_store(&g_store);
    g_scheduler.start([&](const Task& task) {
        std::string chat = task.source_id;
        if (chat.empty()) return;

        if (task.action == "delayed_message") {
            std::string text_to_run = task.parameters.value("text", "");
            nlohmann::json fake_msg;
            try {
                fake_msg["sara_user_id"] = std::stoll(chat);
            } catch (...) {
                fake_msg["sara_user_id"] = 0LL;
            }
            handle_telegram_message(chat, text_to_run, fake_msg);
            return;
        }

        nlohmann::json params = task.parameters;
        if (!task.target.empty() && !params.contains("target"))
            params["target"] = task.target;
        auto res = dispatch_action(task.action, params);
        if (res.success && res.data.contains("path"))
            send_file_result(chat, res, task.action);
        else if (res.success && res.data.contains("output")) {
            std::string out = res.data["output"].get<std::string>();
            if (out.size() > 3500) {
                { std::lock_guard<std::mutex> lock(g_pending_mutex); g_pending_outputs[chat] = out; }
                out = out.substr(0, 3500);
                nlohmann::json btn = {{"text", "📄 Read Full Output"}, {"callback_data", "read_more:" + chat}};
                nlohmann::json kb = {nlohmann::json::array({btn})};
                if (!g_telegram.send_inline_keyboard(chat, res.message + " (truncated)\n`" + out + "`", kb))
                    g_telegram.send_message(chat, res.message + " (truncated)\n`" + out + "`");
            } else {
                g_telegram.send_message(chat, res.message + "\n`" + out + "`");
            }
        } else {
            g_telegram.send_message(chat, (res.success ? "✅ " : "❌ ") + res.message);
        }
    }, cfg.scheduler_interval_ms);

    // ── Start WebSocket Server (GUI/plugins, port 9080) ─────────────────────
    g_ws_server.start(9080);
    g_runtime.set_websocket_ready(true);
    Logger::instance().info("WebSocket server started on port 9080");

    // ── Start Terminal HTTP+WS Server (random port rotation) ──────────────────
    {
        std::string frontend_dir = resolve_path("remote_runtime\\frontend");
        // Shuffle port order so each restart uses a different port immediately
        int all_ports[] = { 9081, 9082, 9083, 9084, 9085 };
        // Simple Fisher-Yates shuffle using system tick count as seed
        unsigned seed = (unsigned)(GetTickCount64() & 0xFFFFFFFF);
        for (int i = 4; i > 0; i--) {
            seed = seed * 1103515245 + 12345;
            int j = (seed >> 16) % (i + 1);
            std::swap(all_ports[i], all_ports[j]);
        }
        // Also try config port first if not in the shuffled list
        bool started = false;
        for (int i = 0; i < 5; i++) {
            int port = all_ports[i];
            sara::remote::TerminalHttpServer::instance().set_proxy_timeouts(
                cfg.proxy_header_timeout_seconds,
                cfg.proxy_idle_timeout_seconds
            );
            if (sara::remote::TerminalHttpServer::instance().start(port, frontend_dir)) {
                sara::remote::TerminalSessionManager::instance().start_cleanup_thread(60);
                g_actual_terminal_port = port;
                Logger::instance().info("Terminal HTTP server started on port " + std::to_string(port));
                started = true;
                break;
            }
            Logger::instance().warning("Port " + std::to_string(port) + " unavailable, trying next...");
        }
        if (!started) {
            Logger::instance().err("Terminal HTTP server failed to start on any port");
        }
    }

    // ── Start File Browser ────────────────────────────────────────────
    if (cfg.filebrowser_enabled) {
        std::string fb_dir = resolve_path("runtime");
        std::string fb_exe = sara::remote::FileBrowserManager::ensure_filebrowser(fb_dir);
        if (!fb_exe.empty()) {
            // Add fb_dir to PATH so filebrowser is findable
            std::string cur_path2(32768, '\0');
            DWORD plen2 = GetEnvironmentVariableA("PATH", cur_path2.data(), (DWORD)cur_path2.size());
            cur_path2.resize(plen2);
            SetEnvironmentVariableA("PATH", (fb_dir + ";" + cur_path2).c_str());

            std::string fb_db = resolve_path("runtime") + "\\filebrowser.db";
            int fb_port = cfg.filebrowser_port;
            sara::remote::FileBrowserManager::instance().configure(fb_port, cfg.filebrowser_root, fb_db);
            // Tell the HTTP server which port filebrowser is on
            sara::remote::TerminalHttpServer::instance().set_filebrowser_port(fb_port);
            Logger::instance().info("File Browser configured for port " + std::to_string(fb_port) +
                                    " (root: " + cfg.filebrowser_root + ") — will start on demand");
        } else {
            Logger::instance().warning("filebrowser.exe not found or download failed — /files unavailable");
        }
    }

    // ── Prepare Cloudflare Tunnel ──────────────────────────────────────────
    if (cfg.cloudflare_mode != "disabled") {
        std::string cf_dir = cfg.cloudflare_exe_dir.empty() 
            ? resolve_path("runtime")
            : cfg.cloudflare_exe_dir;

        // Add cf_dir to PATH so cloudflared is findable
        std::string cf_exe = sara::remote::CloudflaredManager::ensure_cloudflared(cf_dir);
        if (!cf_exe.empty()) {
            // Append to PATH so SearchPathA finds it
            std::string cur_path(32768, '\0');
            DWORD plen = GetEnvironmentVariableA("PATH", cur_path.data(), (DWORD)cur_path.size());
            cur_path.resize(plen);
            SetEnvironmentVariableA("PATH", (cf_dir + ";" + cur_path).c_str());

            Logger::instance().info("cloudflared.exe found: " + cf_exe);
            // Tunnel is NO LONGER started here. It is started on-demand in MessageHandlers.cpp
        } else {
            Logger::instance().warning("cloudflared.exe download failed — tunnel unavailable");
        }
    }

    // ── Start subsystems ──────────────────────────────────────────────────
    g_proc_monitor.start();

    g_event_engine.set_store(&g_store);
    g_event_engine.set_notify_callback([](const std::string& msg) {
        auto chats = g_config.get().telegram.allowed_user_ids;
        if (chats.empty()) return;
        std::string chat_id = std::to_string(chats[0]);
        if (msg.rfind("__FILE__:", 0) == 0) {
            std::string path = msg.substr(9);
            g_telegram.send_photo(chat_id, path, "📷 Event capture");
        } else {
            g_telegram.send_message(chat_id, "[Event] " + msg);
        }
    });
    g_event_engine.start(&g_executor, &g_proc_monitor);

    SpotifyPlugin::instance().start();

    g_net_monitor.on_connect([](const NetworkStatus& s) {
        Logger::instance().info("Network: " + s.ip);
    });
    g_net_monitor.on_disconnect([]() {
        Logger::instance().info("Network disconnected");
    });
    g_net_monitor.start();

    HotkeyManager::instance().start();

    g_ipc.set_handler(handle_ipc_message);
    g_ipc.start();

    g_telegram.set_message_handler(handle_telegram_message);
    g_telegram.start(cfg.telegram.token, cfg.telegram.polling_interval_ms);

    MCPRegistry::instance().load_config(resolve_path("settings\\mcp_servers.json"));

    g_runtime.set_telegram_ready(g_telegram.is_running());
    g_runtime.set_ws_clients(g_ws_server.client_count());

    Logger::instance().info("=== SARA ready. All systems online. ===");

    // ── Main loop ─────────────────────────────────────────────────────────
    while (g_running) {
        Sleep(1000);
        g_runtime.set_ws_clients(g_ws_server.client_count());
        g_runtime.set_active_tasks((int)g_scheduler.list_tasks().size());
    }

    // ── Shutdown ─────────────────────────────────────────────────────
    Logger::instance().info("SARA shutting down...");
    MCPRegistry::instance().shutdown();
    sara::remote::FileBrowserManager::instance().stop();
    g_runtime.set_websocket_ready(false);
    sara::remote::TerminalSessionManager::instance().shutdown_all();
    sara::remote::TerminalSessionManager::instance().stop_cleanup_thread();
    sara::remote::TerminalHttpServer::instance().stop();
    sara::remote::CloudflaredManager::instance().stop();
    g_ws_server.stop();
    SpotifyPlugin::instance().stop();
    g_event_engine.stop();
    g_proc_monitor.stop();
    g_net_monitor.stop();
    HotkeyManager::instance().stop();
    g_telegram.stop();
    g_ipc.stop();
    g_scheduler.stop();
    g_store.close();
    g_runtime.mark_shutdown();
    init.shutdown();
    Logger::instance().shutdown();
    return 0;
}
