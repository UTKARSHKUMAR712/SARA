// SARA Agent v2.0 — Main Entry Point
// Upgraded: Intent Engine, Execution Planner, EventAutomation, ProcessMonitor,
//           NetworkMonitor, HotkeyManager, semantic tool dispatch.
#include "../include/Logger.h"
#include "../include/ConfigManager.h"
#include "../include/Scheduler.h"
#include "../include/SQLiteStore.h"
#include "../include/WinAPIExecutor.h"
#include "../include/TelegramGateway.h"
#include "../include/IPCServer.h"
#include "../include/AIWorkerLauncher.h"
#include "../include/SessionManager.h"
#include "../include/ActionValidator.h"
#include "../include/PermissionManager.h"
#include "../include/IntentEngine.h"
#include "../include/ProcessMonitor.h"
#include "../include/EventAutomation.h"
#include "../include/NetworkMonitor.h"
#include "../include/HotkeyManager.h"
#include "../include/SystemMonitor.h"
#include "../include/DockRouter.h"
#include "../include/SecurityManager.h"
#include "../include/NativeCommandRouter.h"
#include "../../plugins/spotify/spotify_plugin.hpp"
#include <windows.h>
#include <csignal>
#include <atomic>
#include <sstream>
#include <chrono>

using namespace sara;

static std::atomic<bool> g_running{true};
static std::string       g_exe_dir;

static std::string resolve_path(const std::string& path) {
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
AIWorkerLauncher       g_ai_worker;
SessionManager         g_sessions;
ProcessMonitor         g_proc_monitor;
EventAutomationEngine  g_event_engine;
NetworkMonitor         g_net_monitor;

void signal_handler(int) { g_running = false; }

// ── Helper: send file or photo back via Telegram ──────────────────────────────
static void send_file_result(const std::string& chat_id,
                              const ActionResult& res,
                              const std::string& action_name)
{
    if (!res.success || !res.data.contains("path")) return;
    std::string path = res.data["path"].get<std::string>();
    bool is_image = action_name == "take_screenshot"
                 || action_name == "capture_camera"
                 || action_name == "take_photo";
    if (is_image)
        g_telegram.send_photo(chat_id, path, res.message);
    else
        g_telegram.send_document(chat_id, path, res.message);
}

// ── Execute a single plan step from the execution_plan ───────────────────────
static std::string execute_plan_step(const std::string& chat_id,
                                      const json& step,
                                      const std::string& session_id)
{
    if (!step.contains("action")) return "";
    std::string act    = step.value("action", "");
    std::string target = step.value("target", "");
    json params        = step.value("parameters", json::object());
    std::string desc   = step.value("description", act);

    // Record the command so the AI has context of what it just did
    g_sessions.add_command(session_id, step);

    // Merge target into params if not already present
    if (!target.empty() && !params.contains("target"))
        params["target"] = target;

    // Handle delay before execution
    int delay = params.value("delay_seconds", 0);
    if (delay > 0) {
        params.erase("delay_seconds");
        // Schedule it instead of blocking
        Task task;
        task.source    = "telegram";
        task.source_id = chat_id;
        task.action    = act;
        task.target    = target;
        task.parameters = params;
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        task.execute_at = now + delay;
        g_scheduler.add_task(task);
        return "⏱ Scheduled '" + desc + "' in " + std::to_string(delay) + "s\n";
    }

    // ── Special: remember ───────────────────────────────────────────────────
    if (act == "remember") {
        std::string k = params.value("key", "");
        std::string v = params.value("value", "");
        if (!k.empty() && !v.empty()) {
            g_store.memory_set(k, v);
            return "Remembered: " + k + " = " + v + "\n";
        }
        return "";
    }

    // ── Special: event rule management ─────────────────────────────────────
    if (act == "add_event_rule") {
        EventRule rule;
        rule.trigger_type  = params.value("trigger_type", "");
        rule.trigger_value = params.value("trigger_value", "");
        rule.description   = params.value("description", "Custom rule");
        std::string acts   = params.value("actions_json", "[]");
        try { rule.actions = json::parse(acts); }
        catch (...) { rule.actions = json::array(); }

        if (rule.trigger_type.empty())
            return "❌ Error: trigger_type is required for event rules\n";

        g_event_engine.add_rule(rule);
        return "✅ Rule added: " + rule.description
             + " (trigger: " + rule.trigger_type + ":" + rule.trigger_value + ")\n";
    }
    if (act == "remove_event_rule") {
        std::string rid = params.value("rule_id", "");
        if (g_event_engine.remove_rule(rid))
            return "✅ Rule removed\n";
        return "❌ Rule not found\n";
    }
    if (act == "remove_all_event_rules") {
        auto rules = g_event_engine.list_rules();
        for (auto& r : rules) {
            g_event_engine.remove_rule(r.id);
        }
        return "✅ All event rules removed (" + std::to_string(rules.size()) + " total)\n";
    }
    if (act == "list_event_rules") {
        auto rules = g_event_engine.list_rules();
        if (rules.empty()) return "No event rules active.\n";
        std::string out = "Active rules (" + std::to_string(rules.size()) + "):\n";
        for (auto& r : rules) {
            out += (r.enabled ? "✅ " : "❌ ");
            out += "[" + r.id.substr(0, 8) + "] " + r.description
                 + " (" + r.trigger_type + ":" + r.trigger_value + ")\n";
        }
        return out;
    }

    // ── Permission check ────────────────────────────────────────────────────
    if (!PermissionManager::instance().check_action(act, target, "telegram", params)) {
        return "🔒 Permission denied for: " + act + "\n";
    }

    // ── Execute via WinAPIExecutor ──────────────────────────────────────────
    auto res = g_executor.execute(act, params);

    Logger::instance().info("Step [" + act + "]: "
        + (res.success ? "OK" : "FAIL") + " - " + res.message);

    // Visual capture → send photo/document automatically
    if (res.success && res.data.contains("path")) {
        send_file_result(chat_id, res, act);
        return res.message + "\n";
    }

    // Command output
    if (res.success && res.data.contains("output")) {
        std::string out = res.data["output"].get<std::string>();
        if (out.size() > 3000) out = out.substr(0, 3000) + "\n...(truncated)";
        return res.message + "\n```\n" + out + "\n```\n";
    }

    // JSON data (stats, IP, etc.)
    if (res.success && !res.data.is_null() && !res.data.empty()) {
        return res.message + "\n" + res.data.dump(2) + "\n";
    }

    return (res.success ? "✅ " : "❌ ") + res.message + "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN TELEGRAM MESSAGE HANDLER
// ─────────────────────────────────────────────────────────────────────────────
void handle_telegram_message(const std::string& chat_id,
    const std::string& text, const json& raw_message)
{
    long long user_id = raw_message.value("sara_user_id", 0LL);
    std::string from_name = raw_message.contains("from") ? raw_message["from"].value("first_name", "Unknown") : "Unknown";

    Logger::instance().info("Telegram: [" + std::to_string(user_id) + " | " + from_name + "] " + text);

    // 1. Rate Limiting Check
    if (!SecurityManager::instance().check_rate_limit(user_id)) {
        return; // Drop silently to avoid spamming the user
    }

    // 2. Authentication Flow
    if (!SecurityManager::instance().is_trusted_user(user_id)) {
        int msg_id = raw_message.value("message_id", 0);
        if (msg_id > 0) g_telegram.api_call("deleteMessage", {{"chat_id", chat_id}, {"message_id", msg_id}});

        // Extract identity details for rich storage
        std::string tg_username   = "";
        std::string tg_first_name = "";
        std::string tg_last_name  = "";
        if (raw_message.contains("from")) {
            auto& from = raw_message["from"];
            tg_username   = from.value("username",   "");
            tg_first_name = from.value("first_name", "");
            tg_last_name  = from.value("last_name",  "");
        }

        if (SecurityManager::instance().authenticate_root(user_id, text,
                tg_username, tg_first_name, tg_last_name)) {
            g_telegram.send_message(chat_id, "🔓 Device authorized. You are now a trusted user.\nPlease enter the Session Password to begin.");
        } else {
            if (SecurityManager::instance().trigger_root_auth_flow(user_id)) {
                g_telegram.send_message(chat_id, "🔒 Untrusted device.\nPlease provide the Root Password shown on the host computer.");
            } else {
                g_telegram.send_message(chat_id, "🔒 Incorrect Root Password or already pending. Please check the host computer.");
            }
        }
        return;
    }

    if (!SecurityManager::instance().is_session_authenticated(user_id)) {
        int msg_id = raw_message.value("message_id", 0);
        if (msg_id > 0) g_telegram.api_call("deleteMessage", {{"chat_id", chat_id}, {"message_id", msg_id}});

        if (SecurityManager::instance().authenticate_session(user_id, text)) {
            g_telegram.send_message(chat_id, "🔓 Session authenticated. Welcome to SARA Runtime.");
        } else {
            g_telegram.send_message(chat_id, "🔒 Session locked.\nPlease authenticate using your Session Password.");
            Logger::instance().warning("Unauthorized session access attempt by user: " + std::to_string(user_id));
        }
        return;
    }
    
    // ── Handle Inline Keyboard Callbacks ────────────────────────────────────
    if (raw_message.contains("callback_data")) {
        std::string callback_data = raw_message["callback_data"].get<std::string>();
        std::string callback_query_id = raw_message["callback_query_id"].get<std::string>();
        int message_id = raw_message.value("message_id", 0);
        
        if (DockRouter::handle_callback(chat_id, callback_data, callback_query_id, message_id)) {
            return; // Successfully handled by DockRouter, bypass AI
        }
    }

    auto& session = g_sessions.get_or_create("telegram", chat_id);

    // ── Built-in commands ───────────────────────────────────────────────────
    if (text.rfind("/dock", 0) == 0 || text.rfind("/system", 0) == 0 || 
        text.rfind("/monitor", 0) == 0 || text.rfind("/tasks", 0) == 0 || text.rfind("/rules", 0) == 0) {
        if (DockRouter::handle_command(chat_id, text)) {
            return; // Handled
        }
    }

    if (!text.empty() && text[0] == '/') {
        if (NativeCommandRouter::handle(chat_id, text)) {
            return; // Handled by Native OS
        }
    }

    if (text == "/start") {
        g_telegram.send_message(chat_id,
            "🤖 SARA v2.0 — AI Orchestration Runtime\n\n"
            "I'm your intelligent Windows assistant. Talk naturally!\n\n"
            "Examples:\n"
            "• play Jungkook on YouTube\n"
            "• take screenshot when Discord opens\n"
            "• set volume to 50\n"
            "• what's my CPU usage?\n"
            "• remind me in 10 minutes\n\n"
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
            "/memory      — Stored user memory\n"
            "/monitor     — Live system stats\n"
            "/screenshot  — Take screenshot\n"
            "/photo       — Webcam photo\n"
            "/ping        — Latency check\n"
            "/test        — System diagnostic\n"
            "/show_config — AI configuration\n"
            "/set_ai key val — Change AI setting\n\n"
            "Or just talk naturally! 💬");
        return;
    }
    if (text == "/status") {
        SystemMonitor mon;
        auto stats = mon.get_all();
        std::ostringstream oss;
        oss << "🟢 SARA v2.0 — Online\n"
            << "CPU: " << (int)stats.cpu_percent << "%  "
            << "RAM: " << (int)stats.ram_percent << "%\n";
        if (stats.gpu_percent >= 0)
            oss << "GPU: " << (int)stats.gpu_percent << "%\n";
        if (stats.battery_percent >= 0)
            oss << "Battery: " << stats.battery_percent << "% "
                << (stats.on_ac_power ? "(AC)" : "(BAT)") << "\n";
        oss << "Tasks: " << g_scheduler.list_tasks().size()
            << "  Rules: " << g_event_engine.list_rules().size() << "\n"
            << "Telegram: " << (g_telegram.is_running() ? "✅" : "❌") << "\n"
            << "Network: " << (g_net_monitor.get_status().connected ? "✅ " + g_net_monitor.get_status().ip : "❌ disconnected");
        g_telegram.send_message(chat_id, oss.str());
        return;
    }
    if (text == "/ping") {
        g_telegram.send_message(chat_id, "🏓 pong");
        return;
    }
    if (text == "/tasks") {
        auto tasks = g_scheduler.list_tasks();
        if (tasks.empty()) { g_telegram.send_message(chat_id, "No scheduled tasks."); return; }
        std::string reply = "📋 Scheduled Tasks (" + std::to_string(tasks.size()) + "):\n";
        for (auto& t : tasks) {
            reply += "[" + t.id.substr(0, 8) + "] " + t.action;
            if (!t.target.empty()) reply += " → " + t.target;
            reply += " (" + t.status + ")\n";
        }
        g_telegram.send_message(chat_id, reply);
        return;
    }
    if (text.rfind("/cancel ", 0) == 0) {
        std::string id = text.substr(8);
        bool ok = g_scheduler.cancel_task(id);
        g_telegram.send_message(chat_id, ok ? "✅ Task cancelled: " + id : "❌ Task not found: " + id);
        return;
    }
    if (text == "/rules") {
        auto rules = g_event_engine.list_rules();
        if (rules.empty()) { g_telegram.send_message(chat_id, "No event rules active."); return; }
        std::string reply = "⚡ Event Rules (" + std::to_string(rules.size()) + "):\n";
        for (auto& r : rules) {
            reply += (r.enabled ? "✅ " : "❌ ");
            reply += "[" + r.id.substr(0, 8) + "] " + r.description + "\n";
            reply += "    Trigger: " + r.trigger_type + ":" + r.trigger_value + "\n";
        }
        g_telegram.send_message(chat_id, reply);
        return;
    }
    if (text == "/memory") {
        auto mem = g_store.memory_get_all();
        if (mem.empty()) { g_telegram.send_message(chat_id, "No memory stored."); return; }
        std::string reply = "🧠 Stored Memory (" + std::to_string(mem.size()) + "):\n";
        for (auto& [k, v] : mem) reply += "• " + k + " = " + v + "\n";
        g_telegram.send_message(chat_id, reply);
        return;
    }
    if (text == "/monitor") {
        SystemMonitor mon;
        auto j = mon.get_system_summary();
        std::ostringstream oss;
        oss << "📊 System Monitor\n"
            << "CPU: " << (int)j.value("cpu_percent", 0.0) << "%\n"
            << "RAM: " << (int)j.value("ram_percent", 0.0) << "% ("
                << j.value("ram_used_mb", 0) << "/" << j.value("ram_total_mb", 0) << " MB)\n";
        if (j.contains("gpu_percent"))
            oss << "GPU: " << (int)j["gpu_percent"].get<double>() << "%\n";
        if (j.contains("battery_percent"))
            oss << "Battery: " << j["battery_percent"].get<int>() << "%\n";
        if (j.contains("idle_seconds"))
            oss << "Idle: " << j["idle_seconds"].get<int>() << "s\n";
        oss << "Processes: " << j.value("process_count", 0);
        auto net = g_net_monitor.get_status_json();
        oss << "\nNetwork: " << (net.value("connected", false) ? net.value("ip", "?") : "disconnected");
        g_telegram.send_message(chat_id, oss.str());
        return;
    }
    if (text == "/screenshot" || text == "/photo") {
        std::string action = (text == "/photo") ? "capture_camera" : "take_screenshot";
        g_telegram.send_action(chat_id, "upload_photo");
        auto res = g_executor.execute(action, json::object());
        if (res.success && res.data.contains("path")) {
            g_telegram.send_photo(chat_id, res.data["path"].get<std::string>(), "");
        } else {
            g_telegram.send_message(chat_id, res.message);
        }
        return;
    }
    if (text == "/show_config") {
        auto& cfg = g_config.get();
        std::string reply = "⚙️ AI Configuration\n"
            "Provider: " + cfg.ai_primary.provider + "\n"
            "Model: " + cfg.ai_primary.model + "\n"
            "Endpoint: " + cfg.ai_primary.endpoint + "\n"
            "Key: " + (cfg.ai_primary.api_key.size() > 12
                ? cfg.ai_primary.api_key.substr(0, 12) + "..." : cfg.ai_primary.api_key);
        g_telegram.send_message(chat_id, reply);
        return;
    }
    if (text.rfind("/set_ai ", 0) == 0) {
        auto& cfg = g_config.get();
        std::string args  = text.substr(8);
        auto space = args.find(' ');
        if (space == std::string::npos) {
            g_telegram.send_message(chat_id, "Usage: /set_ai provider|model|endpoint|key <value>");
            return;
        }
        std::string param = args.substr(0, space);
        std::string val   = args.substr(space + 1);
        if (param == "provider")       cfg.ai_primary.provider = val;
        else if (param == "model")     cfg.ai_primary.model    = val;
        else if (param == "endpoint")  cfg.ai_primary.endpoint = val;
        else if (param == "key")       cfg.ai_primary.api_key  = val;
        else { g_telegram.send_message(chat_id, "Unknown: " + param); return; }
        g_config.save(resolve_path("settings.json"));
        g_telegram.send_message(chat_id, "✅ AI " + param + " updated to: " + val);
        return;
    }
    if (text == "/test") {
        g_telegram.send_message(chat_id, "🔬 Running SARA v2.0 diagnostic...");
        auto& cfg = g_config.get();
        std::string report;
        auto ai = g_ai_worker.test_provider(cfg.ai_primary);
        report += "AI: " + std::string(ai["success"].get<bool>() ? "✅ OK" : "❌ FAIL") + "\n";
        auto ss = g_executor.execute("take_screenshot", {});
        report += "Screenshot: " + std::string(ss.success ? "✅ OK" : "❌ FAIL") + "\n";
        auto nt = g_executor.execute("notify", {{"target", "SARA Test"}, {"message", "Diagnostic v2.0"}});
        report += "Notify: " + std::string(nt.success ? "✅ OK" : "❌ FAIL") + "\n";
        SystemMonitor mon;
        auto stats = mon.get_all();
        report += "CPU: " + std::to_string((int)stats.cpu_percent) + "% "
                + "RAM: " + std::to_string((int)stats.ram_percent) + "%\n";
        if (stats.gpu_percent >= 0)
            report += "GPU: " + std::to_string((int)stats.gpu_percent) + "%\n";
        report += "Rules: " + std::to_string(g_event_engine.list_rules().size()) + "\n";
        report += "Network: " + (g_net_monitor.get_status().connected
            ? g_net_monitor.get_status().ip : "disconnected");
        g_telegram.send_message(chat_id, report);
        return;
    }

    // ── AI Planner Pipeline ─────────────────────────────────────────────────
    std::string memory_prompt;
    auto mem = g_store.memory_get_all();
    if (!mem.empty()) {
        memory_prompt = "Known user context:\n";
        for (auto& [k, v] : mem) memory_prompt += "- " + k + ": " + v + "\n";
    }

    g_telegram.send_action(chat_id, "typing");
    auto& cfg = g_config.get();
    if (cfg.ai_primary.api_key.empty()) {
        g_telegram.send_message(chat_id,
            "⚠️ No AI configured.\nUse /set_ai key <your_api_key> to set up.\n"
            "Or /show_config to check current settings.");
        return;
    }

    g_sessions.add_history(session.session_id, "user", text);

    // Classify intent for logging
    auto intent = IntentEngine::instance().classify(text);
    Logger::instance().info("Intent: " + intent.category_name
        + " (conf=" + std::to_string(intent.confidence) + ")");

    auto ctx    = g_sessions.get_context(session.session_id);
    auto result = g_ai_worker.plan_command_with_fallback(
        text, cfg.ai_primary, cfg.ai_fallback, ctx, memory_prompt);

    if (result.needs_clarification) {
        g_telegram.send_message(chat_id, result.clarification_question);
        return;
    }
    if (!result.success) {
        g_telegram.send_message(chat_id,
            "❌ AI error: " + result.error + "\nUse /show_config to check settings.");
        Logger::instance().err("AI plan failed: " + result.error);
        return;
    }

    // Send conversational response immediately
    if (!result.response_text.empty()) {
        g_sessions.add_history(session.session_id, "assistant", result.response_text);
        g_telegram.send_message(chat_id, result.response_text);
    }

    // Execute plan steps
    if (!result.execution_plan.is_null() && result.execution_plan.is_array()
        && !result.execution_plan.empty())
    {
        std::string combined;
        for (auto& step : result.execution_plan) {
            combined += execute_plan_step(chat_id, step, session.session_id);
        }
        if (!combined.empty()) {
            if (combined.size() > 4000) combined = combined.substr(0, 4000) + "\n...(truncated)";
            g_telegram.send_message(chat_id, combined);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// IPC Handler (for GUI)
// ─────────────────────────────────────────────────────────────────────────────
json handle_ipc_message(const json& msg) {
    auto type    = msg.value("type", "");
    auto req_id  = msg.value("request_id", "");
    auto payload = msg.value("payload", json::object());

    if (type == "command") {
        std::string action = payload.value("action", "");
        std::string target = payload.value("target", "");
        json params = payload.value("parameters", json::object());
        params["target"] = target;

        if (action == "reload_config") {
            auto& cfg = g_config.get();
            if (payload.contains("telegram")) {
                cfg.telegram.token = payload["telegram"].value("token", cfg.telegram.token);
                g_telegram.stop();
                g_telegram.start(cfg.telegram.token, cfg.telegram.polling_interval_ms);
            }
            if (payload.contains("ai_primary")) {
                auto& ai = payload["ai_primary"];
                cfg.ai_primary.provider = ai.value("provider", cfg.ai_primary.provider);
                cfg.ai_primary.endpoint = ai.value("endpoint", cfg.ai_primary.endpoint);
                cfg.ai_primary.api_key  = ai.value("api_key",  cfg.ai_primary.api_key);
                cfg.ai_primary.model    = ai.value("model",    cfg.ai_primary.model);
            }
            g_config.save(resolve_path("settings.json"));
            return {{"type","response"}, {"request_id",req_id},
                    {"payload", {{"success",true},{"message","Config updated"}}}};
        }
        if (action == "get_root_password") {
            return {{"type","response"}, {"request_id",req_id},
                    {"payload", {{"success",true},{"password", SecurityManager::instance().get_latest_root_password()}}}};
        }
        if (action == "get_users") {
            json users_arr = json::array();
            for (auto& u : SecurityManager::instance().get_trusted_users()) {
                users_arr.push_back({
                    {"user_id",    u.user_id},
                    {"username",   u.username},
                    {"first_name", u.first_name},
                    {"last_name",  u.last_name},
                    {"phone",      u.phone},
                    {"added_at",   u.added_at},
                    {"blocked",    u.blocked}
                });
            }
            return {{"type","response"}, {"request_id",req_id},
                    {"payload", {{"success",true},{"users", users_arr}}}};
        }
        if (action == "block_user") {
            long long uid = payload.value("user_id", 0LL);
            bool ok = SecurityManager::instance().block_user(uid);
            return {{"type","response"}, {"request_id",req_id},
                    {"payload", {{"success",ok}}}};
        }
        if (action == "unblock_user") {
            long long uid = payload.value("user_id", 0LL);
            bool ok = SecurityManager::instance().unblock_user(uid);
            return {{"type","response"}, {"request_id",req_id},
                    {"payload", {{"success",ok}}}};
        }
        if (action == "get_rules") {
            json rules_json = json::array();
            for (auto& r : g_event_engine.list_rules()) {
                rules_json.push_back({
                    {"id",r.id},{"trigger_type",r.trigger_type},
                    {"trigger_value",r.trigger_value},{"enabled",r.enabled},
                    {"description",r.description}});
            }
            return {{"type","response"}, {"request_id",req_id},
                    {"payload",{{"success",true},{"rules",rules_json}}}};
        }

        auto res = g_executor.execute(action, params);
        return {{"type","response"}, {"request_id",req_id},
                {"payload",{{"success",res.success},{"message",res.message},{"data",res.data}}}};
    }

    if (type == "status") {
        SystemMonitor mon;
        return {{"type","status"}, {"request_id",req_id},
                {"payload",{{"running",true},
                            {"tasks",(int)g_scheduler.list_tasks().size()},
                            {"rules",(int)g_event_engine.list_rules().size()},
                            {"telegram",g_telegram.is_running()},
                            {"stats",mon.get_system_summary()}}}};
    }
    if (type == "get_telegram_updates") {
        return {{"type","telegram_updates"}, {"request_id",req_id},
                {"payload",{{"messages",g_telegram.get_recent_messages(20)}}}};
    }
    if (type == "test_ai") {
        auto cfg = g_config.get();
        return {{"type","ai_test_result"}, {"request_id",req_id},
                {"payload",g_ai_worker.test_provider(cfg.ai_primary)}};
    }
    return {{"type","error"}, {"request_id",req_id},
            {"payload",{{"error","unknown message type"}}}};
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    SetConsoleTitleA("SARA Agent v2.0");
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

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
    Logger::instance().info("=== SARA Agent v2.0 starting ===");

    if (!g_config.load(resolve_path("settings.json")))
        Logger::instance().warning("No settings.json — using defaults");

    auto& cfg = g_config.get();
    
    // Initialize security system early
    SecurityManager::instance().initialize(resolve_path(cfg.data_dir));

    if (!g_store.open(resolve_path(cfg.data_dir) + "\\scheduler.db")) {
        Logger::instance().err("Failed to open database");
        return 1;
    }

    g_scheduler.set_store(&g_store);
    g_scheduler.start([&](const Task& task) {
        json params = task.parameters;
        if (!task.target.empty() && !params.contains("target"))
            params["target"] = task.target;
        auto res = g_executor.execute(task.action, params);
        std::string chat = task.source_id;
        if (chat.empty()) return;
        if (res.success && res.data.contains("path"))
            send_file_result(chat, res, task.action);
        else if (res.success && res.data.contains("output")) {
            std::string out = res.data["output"].get<std::string>();
            if (out.size() > 4000) out = out.substr(0, 4000) + "\n...(truncated)";
            g_telegram.send_message(chat, res.message + "\n```\n" + out + "\n```");
        } else {
            g_telegram.send_message(chat, (res.success ? "✅ " : "❌ ") + res.message);
        }
    }, cfg.scheduler_interval_ms);

    // Auto-approve Telegram + GUI commands
    PermissionManager::instance().set_approval_callback(
        [](const std::string&, const std::string&, const std::string& src) {
            return src == "telegram" || src == "gui";
        });

    // ── Start subsystems ────────────────────────────────────────────────────
    g_proc_monitor.start();

    g_event_engine.set_store(&g_store);
    g_event_engine.set_notify_callback([](const std::string& msg) {
        // Find the first telegram chat to notify (simple approach)
        auto chats = g_config.get().telegram.allowed_user_ids;
        if (chats.empty()) return;
        std::string chat_id = std::to_string(chats[0]);

        if (msg.rfind("__FILE__:", 0) == 0) {
            std::string path = msg.substr(9);
            g_telegram.send_photo(chat_id, path, "📷 Event rule visual capture");
        } else {
            g_telegram.send_message(chat_id, msg);
            Logger::instance().info("EventEngine notify: " + msg);
        }
    });
    g_event_engine.start(&g_executor, &g_proc_monitor);
    SpotifyPlugin::instance().start();

    g_net_monitor.on_connect([](const NetworkStatus& s) {
        Logger::instance().info("Network connected: " + s.ip);
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

    Logger::instance().info("=== SARA Agent v2.0 ready. All systems online. ===");

    while (g_running) Sleep(1000);

    Logger::instance().info("SARA Agent shutting down...");
    SpotifyPlugin::instance().stop();
    g_event_engine.stop();
    g_proc_monitor.stop();
    g_net_monitor.stop();
    HotkeyManager::instance().stop();
    g_telegram.stop();
    g_ipc.stop();
    g_scheduler.stop();
    g_store.close();
    Logger::instance().shutdown();
    return 0;
}
