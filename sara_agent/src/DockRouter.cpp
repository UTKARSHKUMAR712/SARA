#include "../include/DockRouter.h"
#include "../include/MediaDock.h"
#include "../include/SystemDock.h"
#include "../include/MonitorDock.h"
#include "../include/AutomationDock.h"
#include "../include/TelegramGateway.h"
#include "../../plugins/spotify/spotify_plugin.hpp"
#include <thread>

extern sara::TelegramGateway g_telegram;

namespace sara {

bool DockRouter::handle_command(const std::string& chat_id, const std::string& command) {
    if (command == "/dock" || command == "/dock media") {
        MediaDock::send_dock(chat_id);
        return true;
    }
    if (command == "/system" || command == "/dock system") {
        SystemDock::send_dock(chat_id);
        return true;
    }
    if (command == "/monitor" || command == "/dock monitor") {
        MonitorDock::send_dock(chat_id);
        return true;
    }
    if (command == "/rules" || command == "/tasks" || command == "/dock auto") {
        AutomationDock::send_dock(chat_id);
        return true;
    }
    return false;
}

bool DockRouter::handle_callback(const std::string& chat_id, const std::string& callback_data, const std::string& callback_query_id, int message_id) {
    // Spotify dock buttons handled first
    if (callback_data.substr(0, 8) == "spotify:") {
        return SpotifyPlugin::instance().handle_callback(
            chat_id, callback_data, callback_query_id, message_id);
    }

    // Expected format: "dock_media:play_pause"
    size_t colon_pos = callback_data.find(':');
    if (colon_pos == std::string::npos) return false;

    std::string prefix = callback_data.substr(0, colon_pos);
    std::string action = callback_data.substr(colon_pos + 1);

    if (prefix == "dock_media") {
        std::thread([=]() {
            MediaDock::handle_action(chat_id, action, callback_query_id);
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

}
