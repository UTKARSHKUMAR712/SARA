#include "../include/DockRouter.h"
#include "../include/MediaDock.h"
#include "../include/SystemDock.h"
#include "../include/MonitorDock.h"
#include "../include/AutomationDock.h"
#include "../include/TelegramGateway.h"
#include "../include/Logger.h"
#include "../../plugins/spotify/spotify_plugin.hpp"
#include <thread>

extern sara::TelegramGateway g_telegram;

namespace sara {

bool DockRouter::handle_command(const std::string& chat_id, const std::string& command) {
    Logger::instance().info("[DockRouter] handle_command: '" + command + "' from " + chat_id);

    if (command == "/dock") {
        // Master dock menu using send_inline_keyboard (correct API)
        json kb = json::array({
            json::array({
                {{"text", "\xF0\x9F\x96\xA5\xEF\xB8\x8F System"},  {"callback_data", "dock_main:sys"}},
                {{"text", "\xF0\x9F\x93\x8A Monitor"},              {"callback_data", "dock_main:mon"}}
            }),
            json::array({
                {{"text", "\xF0\x9F\x8E\xB5 Media"},       {"callback_data", "dock_main:media"}},
                {{"text", "\xE2\x9A\x99\xEF\xB8\x8F Automation"}, {"callback_data", "dock_main:auto"}}
            }),
            json::array({
                {{"text", "\xF0\x9F\x9F\xA2 Spotify"}, {"callback_data", "dock_main:sp"}}
            })
        });
        Logger::instance().info("[DockRouter] Sending master dock menu");
        g_telegram.send_inline_keyboard(chat_id, "\xF0\x9F\x8E\x9B\xEF\xB8\x8F SARA Master Control Dock\nSelect a module:", kb);
        return true;
    }

    if (command == "/media" || command == "/dock media") {
        Logger::instance().info("[DockRouter] Routing to MediaDock");
        MediaDock::instance().send_dock(chat_id);
        return true;
    }
    if (command == "/system" || command == "/dock system") {
        Logger::instance().info("[DockRouter] Routing to SystemDock");
        SystemDock::send_dock(chat_id);
        return true;
    }
    if (command == "/monitor" || command == "/dock monitor") {
        Logger::instance().info("[DockRouter] Routing to MonitorDock");
        MonitorDock::send_dock(chat_id);
        return true;
    }
    if (command == "/rules" || command == "/tasks" || command == "/dock auto") {
        Logger::instance().info("[DockRouter] Routing to AutomationDock");
        AutomationDock::send_dock(chat_id);
        return true;
    }

    Logger::instance().info("[DockRouter] No match for: " + command);
    return false;
}

bool DockRouter::handle_callback(const std::string& chat_id, const std::string& callback_data, const std::string& callback_query_id, int message_id) {
    Logger::instance().info("[DockRouter] handle_callback: '" + callback_data + "'");

    // Spotify dock buttons handled first
    if (callback_data.size() >= 8 && callback_data.substr(0, 8) == "spotify:") {
        return SpotifyPlugin::instance().handle_callback(chat_id, callback_data, callback_query_id, message_id);
    }

    size_t colon_pos = callback_data.find(':');
    if (colon_pos == std::string::npos) return false;

    std::string prefix = callback_data.substr(0, colon_pos);
    std::string action = callback_data.substr(colon_pos + 1);

    if (prefix == "dock_main") {
        Logger::instance().info("[DockRouter] dock_main action: " + action);
        if (action == "sys")        SystemDock::send_dock(chat_id);
        else if (action == "mon")   MonitorDock::send_dock(chat_id);
        else if (action == "auto")  AutomationDock::send_dock(chat_id);
        else if (action == "media") MediaDock::instance().send_dock(chat_id);
        else if (action == "sp")    SpotifyPlugin::instance().handle_command(chat_id, "/sp");
        return true;
    }

    if (prefix == "dock_media") {
        // Format: dock_media:action:session_id  OR  dock_media:action:
        std::string action_and_sid = action; // everything after first colon
        std::string media_action, session_id;
        size_t second_colon = action_and_sid.find(':');
        if (second_colon != std::string::npos) {
            media_action = action_and_sid.substr(0, second_colon);
            session_id   = action_and_sid.substr(second_colon + 1);
        } else {
            media_action = action_and_sid;
            session_id   = "";
        }
        Logger::instance().info("[DockRouter] dock_media action=" + media_action + " session=" + session_id);
        std::thread([=]() {
            MediaDock::instance().handle_action(chat_id, media_action, callback_query_id, message_id, session_id);
        }).detach();
        return true;
    }
    
    if (prefix == "dock_sys") {
        std::thread([=]() {
            SystemDock::handle_action(chat_id, action, callback_query_id, message_id);
        }).detach();
        return true;
    }

    if (prefix == "dock_mon") {
        std::thread([=]() {
            MonitorDock::handle_action(chat_id, action, callback_query_id, message_id);
        }).detach();
        return true;
    }

    if (prefix == "dock_auto") {
        std::thread([=]() {
            AutomationDock::handle_action(chat_id, action, callback_query_id, message_id);
        }).detach();
        return true;
    }

    return false;
}

} // namespace sara
