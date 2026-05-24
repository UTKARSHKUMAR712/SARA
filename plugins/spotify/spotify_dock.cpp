#include "spotify_dock.hpp"
#include "spotify_state.hpp"
#include "spotify_ws.hpp"
#include "spotify_commands.hpp"
#include "../../sara_agent/include/TelegramGateway.h"
#include "../../sara_agent/include/Logger.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

extern sara::TelegramGateway g_telegram;

namespace sara {

SpotifyDock& SpotifyDock::instance() {
    static SpotifyDock inst;
    return inst;
}

void SpotifyDock::stop() {
    running_ = false;
    if (update_thread_.joinable()) update_thread_.join();
}

// ─── Progress Bar ─────────────────────────────────────────────────────────────

std::string SpotifyDock::render_progress_bar(int progress_ms, int duration_ms) {
    if (duration_ms <= 0) return "\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2588";
    int filled = (int)(13.0 * progress_ms / duration_ms);
    filled = std::max(0, std::min(13, filled));
    std::string bar;
    for (int i = 0; i < 13; i++)
        bar += (i < filled) ? "\u2588" : "\u2591";
    return bar;
}

// ─── Render Message Text ──────────────────────────────────────────────────────

std::string SpotifyDock::render_text() {
    auto s = SpotifyStateManager::instance().get();

    if (!s.connected)
        return "\u274c Spotify disconnected\nOpen Spotify to reconnect.";

    if (s.title.empty())
        return "\u23f8 Nothing playing.";

    auto ms_fmt = [](int ms) -> std::string {
        int total = ms / 1000;
        int m = total / 60, sec = total % 60;
        std::ostringstream o;
        o << m << ":" << std::setw(2) << std::setfill('0') << sec;
        return o.str();
    };

    std::string repeat_str = s.repeat_mode == 0 ? "OFF" : s.repeat_mode == 1 ? "ALL" : "ONE";
    std::string play_icon  = s.playing ? "\u25b6\ufe0f" : "\u23f8";

    return
        "\ud83c\udfb5 *NOW PLAYING*\n\n"
        "*" + s.title  + "*\n"
        + s.artist + "\n\n"
        + ms_fmt(s.progress_ms) + " / " + ms_fmt(s.duration_ms) + "\n"
        + render_progress_bar(s.progress_ms, s.duration_ms) + "\n\n"
        "\ud83d\udd0a " + std::to_string(s.volume) + "%   "
        "\ud83d\udd00 " + std::string(s.shuffle ? "ON" : "OFF") + "   "
        "\ud83d\udd01 " + repeat_str + "   "
        "\u2764\ufe0f " + std::string(s.hearted ? "YES" : "NO");
}

// ─── Inline Keyboard ─────────────────────────────────────────────────────────

static nlohmann::json make_keyboard() {
    using json = nlohmann::json;
    return json{
        {"inline_keyboard", json::array({
            json::array({
                {{"text","\u23ee"},{"callback_data","spotify:prev"}},
                {{"text","\u23ef"},{"callback_data","spotify:toggleplay"}},
                {{"text","\u23ed"},{"callback_data","spotify:next"}}
            }),
            json::array({
                {{"text","-10s"},{"callback_data","spotify:backward"}},
                {{"text","+10s"},{"callback_data","spotify:forward"}}
            }),
            json::array({
                {{"text","\u2764\ufe0f"},{"callback_data","spotify:toggleheart"}},
                {{"text","\ud83d\udd00"},{"callback_data","spotify:shuffle_toggle"}},
                {{"text","\ud83d\udd01"},{"callback_data","spotify:repeat_cycle"}}
            }),
            json::array({
                {{"text","\ud83d\udd0a\u2796"},{"callback_data","spotify:vol_down"}},
                {{"text","\ud83d\udd0a\u2795"},{"callback_data","spotify:vol_up"}}
            })
        })}
    };
}

// ─── Send Dock ────────────────────────────────────────────────────────────────

void SpotifyDock::send_dock(const std::string& chat_id) {
    std::string text = render_text();
    auto keyboard    = make_keyboard();

    auto resp = g_telegram.api_call("sendMessage", {
        {"chat_id",    chat_id},
        {"text",       text},
        {"parse_mode", "Markdown"},
        {"reply_markup", keyboard}
    });

    int msg_id = 0;
    if (resp.contains("result") && resp["result"].contains("message_id"))
        msg_id = resp["result"]["message_id"].get<int>();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        sessions_[chat_id] = {chat_id, msg_id};
    }

    // Start update loop if not already running
    if (!running_) {
        running_ = true;
        if (update_thread_.joinable()) update_thread_.join();
        update_thread_ = std::thread(&SpotifyDock::update_loop, this);
    }
}

// ─── Update Loop ─────────────────────────────────────────────────────────────

void SpotifyDock::update_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::string text = render_text();
        auto keyboard    = make_keyboard();

        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& [cid, sess] : sessions_) {
            if (sess.message_id == 0) continue;
            g_telegram.api_call("editMessageText", {
                {"chat_id",    cid},
                {"message_id", sess.message_id},
                {"text",       text},
                {"parse_mode", "Markdown"},
                {"reply_markup", keyboard}
            });
        }
    }
}

// ─── Button Callbacks ─────────────────────────────────────────────────────────

bool SpotifyDock::handle_callback(const std::string& chat_id,
                                   const std::string& callback_data,
                                   const std::string& callback_query_id,
                                   int message_id) {
    if (callback_data.substr(0, 8) != "spotify:") return false;
    std::string action = callback_data.substr(8);

    // Map callback → WS command
    json cmd;
    if      (action == "prev")           cmd = {{"action","prev"}};
    else if (action == "toggleplay")     cmd = {{"action","toggleplay"}};
    else if (action == "next")           cmd = {{"action","next"}};
    else if (action == "backward")       cmd = {{"action","backward"},{"ms",10000}};
    else if (action == "forward")        cmd = {{"action","forward"}, {"ms",10000}};
    else if (action == "toggleheart")    cmd = {{"action","toggleheart"}};
    else if (action == "vol_down") {
        int v = std::max(0, SpotifyStateManager::instance().get().volume - 5);
        cmd = {{"action","volume"},{"value",v}};
    }
    else if (action == "vol_up") {
        int v = std::min(100, SpotifyStateManager::instance().get().volume + 5);
        cmd = {{"action","volume"},{"value",v}};
    }
    else if (action == "shuffle_toggle") {
        bool cur = SpotifyStateManager::instance().get().shuffle;
        cmd = {{"action", cur ? "shuffle_off" : "shuffle_on"}};
    }
    else if (action == "repeat_cycle") {
        int cur = SpotifyStateManager::instance().get().repeat_mode;
        std::string ra = cur == 0 ? "repeat_all" : cur == 1 ? "repeat_one" : "repeat_off";
        cmd = {{"action", ra}};
    }
    else return false;

    SpotifyWS::instance().send_command(cmd);

    // Acknowledge the button press (removes spinner)
    g_telegram.api_call("answerCallbackQuery", {
        {"callback_query_id", callback_query_id},
        {"text", ""}
    });

    return true;
}

} // namespace sara
