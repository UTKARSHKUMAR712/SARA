#include "../include/AutomationDock.h"
#include "../include/TelegramGateway.h"
#include "../include/EventAutomation.h"
#include "../include/Scheduler.h"
#include <sstream>

extern sara::TelegramGateway g_telegram;
extern sara::EventAutomationEngine g_event_engine;
extern sara::Scheduler g_scheduler;

namespace sara {

std::string generate_auto_text() {
    std::ostringstream oss;
    oss << "⚡ **AUTOMATION & TASKS**\n\n";
    
    auto rules = g_event_engine.list_rules();
    auto tasks = g_scheduler.list_tasks();
    
    if (rules.empty() && tasks.empty()) {
        oss << "No active rules or scheduled tasks.\n";
    }
    
    for (const auto& r : rules) {
        oss << "[RULE] " << r.description << "\n"
            << "Status: " << (r.enabled ? "🟢 ENABLED" : "🔴 DISABLED") << "\n\n";
    }
    
    for (const auto& t : tasks) {
        oss << "[TASK] " << t.action << (t.target.empty() ? "" : " -> " + t.target) << "\n"
            << "Status: ⏳ " << t.status << "\n\n";
    }
    
    return oss.str();
}

json get_auto_keyboard() {
    auto rules = g_event_engine.list_rules();
    auto tasks = g_scheduler.list_tasks();
    
    json kb = json::array();
    
    for (const auto& r : rules) {
        std::string txt = (r.enabled ? "🔴 Disable: " : "🟢 Enable: ") + r.description.substr(0, 15);
        std::string data = "dock_auto:toggle_" + r.id;
        kb.push_back(json::array({ json{{"text", txt}, {"callback_data", data}} }));
    }
    
    for (const auto& t : tasks) {
        std::string txt = "❌ Cancel Task: " + t.action.substr(0, 10);
        std::string data = "dock_auto:cancel_" + t.id;
        kb.push_back(json::array({ json{{"text", txt}, {"callback_data", data}} }));
    }
    
    kb.push_back(json::array({ json{{"text", "🔄 Refresh"}, {"callback_data", "dock_auto:refresh"}} }));
    
    return kb;
}

void AutomationDock::send_dock(const std::string& chat_id) {
    g_telegram.send_inline_keyboard(chat_id, generate_auto_text(), get_auto_keyboard());
}

void AutomationDock::handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id) {
    if (action == "refresh") {
        g_telegram.edit_message_text(chat_id, message_id, generate_auto_text(), get_auto_keyboard());
        g_telegram.answer_callback_query(callback_query_id, "Refreshed");
    }
    else if (action.find("toggle_") == 0) {
        std::string id = action.substr(7);
        auto* rule = g_event_engine.find_rule(id);
        if (rule) {
            bool new_state = !rule->enabled;
            g_event_engine.toggle_rule(id, new_state);
            g_telegram.answer_callback_query(callback_query_id, new_state ? "Enabled" : "Disabled");
            g_telegram.edit_message_text(chat_id, message_id, generate_auto_text(), get_auto_keyboard());
        } else {
            g_telegram.answer_callback_query(callback_query_id, "Rule not found");
        }
    }
    else if (action.find("cancel_") == 0) {
        std::string id = action.substr(7);
        if (g_scheduler.cancel_task(id)) {
            g_telegram.answer_callback_query(callback_query_id, "Task Cancelled");
            g_telegram.edit_message_text(chat_id, message_id, generate_auto_text(), get_auto_keyboard());
        } else {
            g_telegram.answer_callback_query(callback_query_id, "Task not found");
        }
    }
}

}
