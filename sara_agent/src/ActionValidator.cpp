#include "../include/ActionValidator.h"
#include "../include/Logger.h"
#include <unordered_map>
#include <algorithm>

namespace sara {

ActionValidator& ActionValidator::instance() {
    static ActionValidator inst;
    return inst;
}

ActionValidator::ActionValidator() {
    // ── AI-visible semantic tools (what the planner can call) ────────────────
    allowed_actions_ = {
        // Browser / media (semantic)
        "play_youtube", "search_google", "open_website",
        "media_play_pause", "media_next", "media_prev", "media_stop",
        "spotify_play", "spotify_pause", "spotify_resume", "spotify_next",
        "spotify_prev", "spotify_volume", "spotify_get_status", "spotify_dock",
        // Apps
        "open_app", "close_process", "focus_window",
        "enum_windows", "enum_processes",
        // Capture / surveillance
        "take_screenshot", "capture_camera", "take_photo",
        // System control
        "notify", "volume_set", "volume_mute", "change_brightness",
        // Clipboard
        "clipboard_write", "clipboard_read",
        // Info / monitoring
        "get_system_stats", "get_ip_address",
        // Event automation
        "add_event_rule", "remove_event_rule", "list_event_rules",
        // Memory
        "remember",
        // Scheduling (internal helper, called by orchestrator)
        "schedule_task", "cancel_task", "list_tasks",
        // Config
        "reload_config", "get_plugin_info",
        // INTERNAL — callable by orchestration engine only, NOT by AI directly
        // (still listed here so the permission system works):
        "send_keys", "type_text", "run_cmd", "run_powershell", "run_bat",
        "mouse_move", "mouse_click",
    };
}

RiskLevel ActionValidator::get_risk_level(const std::string& action) const {
    return classify_action(action);
}

bool ActionValidator::is_action_allowed(const std::string& action) const {
    for (auto& a : allowed_actions_) {
        if (a == action) return true;
    }
    return false;
}

void ActionValidator::add_allowed_action(const std::string& action) {
    allowed_actions_.push_back(action);
}

void ActionValidator::remove_allowed_action(const std::string& action) {
    allowed_actions_.erase(
        std::remove(allowed_actions_.begin(), allowed_actions_.end(), action),
        allowed_actions_.end());
}

RiskLevel ActionValidator::classify_action(const std::string& action) const {
    static const std::unordered_map<std::string, RiskLevel> risk_map = {
        // LOW — safe semantic tools
        {"play_youtube",      RiskLevel::LOW},
        {"spotify_play",      RiskLevel::LOW},
        {"spotify_pause",     RiskLevel::LOW},
        {"spotify_resume",    RiskLevel::LOW},
        {"spotify_next",      RiskLevel::LOW},
        {"spotify_prev",      RiskLevel::LOW},
        {"spotify_volume",    RiskLevel::LOW},
        {"spotify_get_status", RiskLevel::LOW},
        {"spotify_dock",      RiskLevel::LOW},
        {"search_google",     RiskLevel::LOW},
        {"open_website",      RiskLevel::LOW},
        {"media_play_pause",  RiskLevel::LOW},
        {"media_next",        RiskLevel::LOW},
        {"media_prev",        RiskLevel::LOW},
        {"media_stop",        RiskLevel::LOW},
        {"open_app",          RiskLevel::LOW},
        {"take_screenshot",   RiskLevel::LOW},
        {"capture_camera",    RiskLevel::LOW},
        {"take_photo",        RiskLevel::LOW},
        {"notify",            RiskLevel::LOW},
        {"volume_set",        RiskLevel::LOW},
        {"volume_mute",       RiskLevel::LOW},
        {"change_brightness", RiskLevel::LOW},
        {"clipboard_write",   RiskLevel::LOW},
        {"get_system_stats",  RiskLevel::LOW},
        {"get_ip_address",    RiskLevel::LOW},
        {"remember",          RiskLevel::LOW},
        {"focus_window",      RiskLevel::LOW},
        {"enum_windows",      RiskLevel::LOW},
        {"enum_processes",    RiskLevel::LOW},
        {"list_tasks",        RiskLevel::LOW},
        {"list_event_rules",  RiskLevel::LOW},
        {"get_plugin_info",   RiskLevel::LOW},
        // MEDIUM
        {"close_process",     RiskLevel::MEDIUM},
        {"send_keys",         RiskLevel::MEDIUM},
        {"type_text",         RiskLevel::MEDIUM},
        {"mouse_move",        RiskLevel::MEDIUM},
        {"mouse_click",       RiskLevel::MEDIUM},
        {"clipboard_read",    RiskLevel::MEDIUM},
        {"schedule_task",     RiskLevel::MEDIUM},
        {"cancel_task",       RiskLevel::MEDIUM},
        {"reload_config",     RiskLevel::MEDIUM},
        {"add_event_rule",    RiskLevel::MEDIUM},
        {"remove_event_rule", RiskLevel::MEDIUM},
        // HIGH — shell execution
        {"run_cmd",           RiskLevel::HIGH},
        {"run_powershell",    RiskLevel::HIGH},
        {"run_bat",           RiskLevel::HIGH},
    };
    auto it = risk_map.find(action);
    if (it != risk_map.end()) return it->second;
    return RiskLevel::UNKNOWN;
}

ValidationResult ActionValidator::validate(const std::string& action,
    const std::string& target, const json& parameters)
{
    ValidationResult result;
    if (!is_action_allowed(action)) {
        result.valid  = false;
        result.risk   = RiskLevel::UNKNOWN;
        result.reason = "Action not in allowed list: " + action;
        Logger::instance().warning(result.reason);
        return result;
    }
    result.risk  = classify_action(action);
    result.valid = true;
    return result;
}

} // namespace sara
