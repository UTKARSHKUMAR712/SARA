#include "../include/ActionDispatcher.h"
#include "../include/TelegramGateway.h"
#include "../include/WinAPIExecutor.h"
#include "../include/ToolRegistry.h"
#include "../../plugins/spotify/spotify_commands.hpp"
#include <mutex>
#include <unordered_map>
#include <string>


using namespace sara;
extern TelegramGateway        g_telegram;
extern WinAPIExecutor         g_executor;

std::unordered_map<std::string, std::string> g_pending_outputs;
std::mutex g_pending_mutex;
std::string g_last_playlist;
std::mutex g_last_playlist_mutex;

namespace sara {


void send_file_result(const std::string& chat_id,
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

ActionResult dispatch_action(const std::string& act, const nlohmann::json& params) {
    if (act == "media_command") {
        std::string sub_act = params.value("action", "");
        if (sub_act.empty()) return {false, "media_command: missing 'action' field", {}};
        nlohmann::json sub_params = params;
        sub_params.erase("action");
        return dispatch_action(sub_act, sub_params);
    }

    auto spotify_dispatch = [](const std::string& sub, const std::string& args) -> ActionResult {
        std::string res_str = SpotifyCommands::dispatch(sub, args);
        bool ok = (res_str.find("\u274c") == std::string::npos);
        return {ok, res_str, {}};
    };
    if (act == "spotify_play")      return spotify_dispatch("play",   params.value("query", params.value("target", "")));
    if (act == "spotify_playlist")  return spotify_dispatch("playlist", params.value("query", params.value("target", "")));
    if (act == "spotify_radio")     return spotify_dispatch("radio",   params.value("query", params.value("target", "")));
    if (act == "spotify_pause")     return spotify_dispatch("pause",  "");
    if (act == "spotify_resume")    return spotify_dispatch("resume", "");
    if (act == "spotify_toggle")    return spotify_dispatch("toggle", "");
    if (act == "spotify_next")      return spotify_dispatch("next",   "");
    if (act == "spotify_prev")      return spotify_dispatch("prev",   "");
    if (act == "spotify_stop")      return spotify_dispatch("pause",  "");
    if (act == "spotify_shuffle_on")  return spotify_dispatch("shuffle", "on");
    if (act == "spotify_shuffle_off") return spotify_dispatch("shuffle", "off");
    if (act == "spotify_repeat_off")  return spotify_dispatch("repeat", "off");
    if (act == "spotify_repeat_all")  return spotify_dispatch("repeat", "all");
    if (act == "spotify_repeat_one")  return spotify_dispatch("repeat", "one");
    if (act == "spotify_heart")       return spotify_dispatch("heart", "");
    if (act == "spotify_unheart")     return spotify_dispatch("unheart", "");
    if (act == "spotify_volume") {
        int level = params.value("level", params.value("value", 50));
        return spotify_dispatch("volume", std::to_string(level));
    }
    if (act == "spotify_get_status" || act == "spotify_status") return spotify_dispatch("status", "");
    if (act == "spotify_dock") {
        return {true, "Use /sp dock in Telegram for interactive Spotify control", {}};
    }

    if (ToolRegistry::instance().has_tool(act)) {
        nlohmann::json jres = ToolRegistry::instance().execute(act, params);
        ActionResult res;
        res.success = jres.value("success", false);
        if (jres.contains("error") && !jres["error"].is_null()) {
            res.message = jres["error"].get<std::string>();
        } else if (jres.contains("message") && !jres["message"].is_null()) {
            res.message = jres["message"].get<std::string>();
        } else if (jres.contains("data") && jres["data"].is_object() && jres["data"].contains("message") && !jres["data"]["message"].is_null()) {
            res.message = jres["data"]["message"].get<std::string>();
        } else {
            res.message = "";
        }
        if (jres.contains("data") && !jres["data"].is_null()) res.data = jres["data"];
        return res;
    }
    return g_executor.execute(act, params);
}

} // namespace sara
