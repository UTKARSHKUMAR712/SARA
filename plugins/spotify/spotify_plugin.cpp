#include "spotify_plugin.hpp"
#include "spotify_ws.hpp"
#include "spotify_commands.hpp"
#include "spotify_dock.hpp"
#include "spotify_state.hpp"
#include "../../sara_agent/include/TelegramGateway.h"
#include "../../sara_agent/include/Logger.h"
#include "../../sara_agent/include/ToolRegistry.h"
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

    auto register_sp = [](const std::string& sub, const std::string& desc, const json& schema = json::object()) {
        ToolDef def;
        def.name = "spotify_" + sub;
        def.description = desc;
        def.category = ToolCategory::media;
        def.risk_level = "LOW";
        def.ai_visible = true;
        def.parameters_schema = schema;
        def.handler = [sub](const json& p) -> json {
            std::string args = "";
            int seek_sec = -1;
            if ((sub == "play" || sub == "queue" || sub == "playlist" || sub == "radio") && p.contains("query")) {
                args = p["query"].get<std::string>();
                if (sub == "play" && p.contains("seek_seconds")) {
                    seek_sec = p["seek_seconds"].get<int>();
                }
            } else if (sub == "volume" && p.contains("level")) {
                args = std::to_string(p["level"].get<int>());
            } else if ((sub == "shuffle" || sub == "repeat") && p.contains("mode")) {
                args = p["mode"].get<std::string>();
            } else if ((sub == "seek" || sub == "forward" || sub == "backward") && p.contains("seconds")) {
                args = std::to_string(p["seconds"].get<int>());
            }
            
            std::string cmd_sub = sub;
            if (cmd_sub == "get_status") cmd_sub = "status";
            
            if (cmd_sub == "dock") {
                return {{"success", true}, {"message", "Spotify Dock requested"}};
            }

            std::string old_title = SpotifyStateManager::instance().get().title;
            std::string reply = SpotifyCommands::dispatch(cmd_sub, args);

            bool success = true;
            if (reply.find("\u274c") != std::string::npos) {
                success = false;
            }

            if (success && seek_sec >= 0) {
                int wait_loops = 50; // max 5 seconds
                while (wait_loops-- > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    auto s = SpotifyStateManager::instance().get();
                    if (s.title != old_title && !s.title.empty()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        break;
                    }
                }
                SpotifyCommands::dispatch("seek", std::to_string(seek_sec));
            }

            return {{"success", success}, {"message", reply}};
        };
        ToolRegistry::instance().register_tool(def);
    };

    json query_schema = {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Search query or name"}}},
            {"seek_seconds", {{"type", "integer"}, {"description", "Optional: start song from this second"}}}
        }},
        {"required", json::array({"query"})}
    };
    json empty_schema = {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}};

    register_sp("play", "Play song on Spotify", query_schema);
    register_sp("pause", "Pause Spotify", empty_schema);
    register_sp("resume", "Resume Spotify", empty_schema);
    register_sp("next", "Next song in Spotify", empty_schema);
    register_sp("prev", "Prev song in Spotify", empty_schema);
    
    register_sp("volume", "Set volume", {
        {"type", "object"},
        {"properties", {{"level", {{"type", "integer"}}}}},
        {"required", json::array({"level"})}
    });
    
    register_sp("get_status", "Get Spotify status", empty_schema);
    
    register_sp("heart", "Like song", empty_schema);
    register_sp("unheart", "Unlike song", empty_schema);
    
    register_sp("shuffle", "Toggle shuffle", {
        {"type", "object"},
        {"properties", {{"mode", {{"type", "string"}}}}},
        {"required", json::array({"mode"})}
    });
    
    register_sp("repeat", "Set repeat mode", {
        {"type", "object"},
        {"properties", {{"mode", {{"type", "string"}}}}},
        {"required", json::array({"mode"})}
    });
    
    json seconds_schema = {
        {"type", "object"},
        {"properties", {{"seconds", {{"type", "integer"}}}} },
        {"required", json::array({"seconds"})}
    };
    register_sp("seek", "Seek to second", seconds_schema);
    register_sp("forward", "Skip forward", seconds_schema);
    register_sp("backward", "Skip backward", seconds_schema);
    
    register_sp("queue", "Queue song", query_schema);
    register_sp("playlist", "Play playlist", query_schema);
    register_sp("radio", "Play radio", query_schema);
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
    g_telegram.send_message(chat_id, reply, "Markdown");
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
