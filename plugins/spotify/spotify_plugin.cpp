#include "spotify_plugin.hpp"
#include "spotify_ws.hpp"
#include "spotify_commands.hpp"
#include "spotify_dock.hpp"
#include "../../sara_agent/include/TelegramGateway.h"
#include "../../sara_agent/include/Logger.h"
#include <sstream>

extern sara::TelegramGateway g_telegram;

namespace sara {

SpotifyPlugin& SpotifyPlugin::instance() {
    static SpotifyPlugin inst;
    return inst;
}

void SpotifyPlugin::start() {
    SpotifyWS::instance().start();
    Logger::instance().info("SpotifyPlugin: started");
}

void SpotifyPlugin::stop() {
    SpotifyDock::instance().stop();
    SpotifyWS::instance().stop();
    Logger::instance().info("SpotifyPlugin: stopped");
}

bool SpotifyPlugin::handle_command(const std::string& chat_id, const std::string& text) {
    // Must start with /sp
    if (text.size() < 3 || text.substr(0, 3) != "/sp") return false;

    // /sp dock — open live Telegram dock
    if (text == "/sp dock") {
        SpotifyDock::instance().send_dock(chat_id);
        return true;
    }

    // Parse: /sp <sub> [args...]
    std::string rest = text.size() > 4 ? text.substr(4) : "";
    std::string sub, args;
    auto space = rest.find(' ');
    if (space == std::string::npos) {
        sub = rest;
    } else {
        sub  = rest.substr(0, space);
        args = rest.substr(space + 1);
    }

    std::string reply = SpotifyCommands::dispatch(sub, args);
    g_telegram.send_message(chat_id, reply);
    return true;
}

bool SpotifyPlugin::handle_callback(const std::string& chat_id,
                                     const std::string& callback_data,
                                     const std::string& callback_query_id,
                                     int message_id) {
    return SpotifyDock::instance().handle_callback(
        chat_id, callback_data, callback_query_id, message_id);
}

} // namespace sara
