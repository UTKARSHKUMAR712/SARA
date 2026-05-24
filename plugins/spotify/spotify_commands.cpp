#include "spotify_commands.hpp"
#include "spotify_ws.hpp"
#include "spotify_state.hpp"
#include <sstream>
#include <iomanip>

namespace sara {

static std::string ms_to_time(int ms) {
    int total_sec = ms / 1000;
    int m = total_sec / 60;
    int s = total_sec % 60;
    std::ostringstream oss;
    oss << m << ":" << std::setw(2) << std::setfill('0') << s;
    return oss.str();
}

std::string SpotifyCommands::not_connected() {
    return "\u274c Spotify not connected.\nMake sure Spotify is open with the SARA extension loaded.";
}

std::string SpotifyCommands::send(const json& cmd) {
    if (!SpotifyWS::instance().send_command(cmd))
        return not_connected();
    return "\u2705 Done";
}

std::string SpotifyCommands::dispatch(const std::string& sub, const std::string& args) {

    // ── PLAY ──────────────────────────────────────────────────────────────────
    if (sub == "play") {
        if (args.empty()) return send({{"action","resume"}});
        return send({{"action","play"},{"query",args}});
    }

    // ── PAUSE / RESUME ────────────────────────────────────────────────────────
    if (sub == "pause")  return send({{"action","pause"}});
    if (sub == "resume") return send({{"action","resume"}});
    if (sub == "toggle") return send({{"action","toggleplay"}});

    // ── SKIP ──────────────────────────────────────────────────────────────────
    if (sub == "next")   return send({{"action","next"}});
    if (sub == "prev")   return send({{"action","prev"}});

    // ── SEEK ──────────────────────────────────────────────────────────────────
    if (sub == "seek") {
        try {
            int sec = std::stoi(args);
            return send({{"action","seek"},{"ms", sec * 1000}});
        } catch (...) { return "\u274c Usage: /sp seek <seconds>"; }
    }
    if (sub == "forward") {
        int sec = args.empty() ? 10 : std::stoi(args);
        return send({{"action","forward"},{"ms", sec * 1000}});
    }
    if (sub == "backward") {
        int sec = args.empty() ? 10 : std::stoi(args);
        return send({{"action","backward"},{"ms", sec * 1000}});
    }

    // ── VOLUME ────────────────────────────────────────────────────────────────
    if (sub == "volume" || sub == "vol") {
        try {
            int v = std::stoi(args);
            if (v < 0 || v > 100) return "\u274c Volume must be 0-100";
            return send({{"action","volume"},{"value", v}});
        } catch (...) { return "\u274c Usage: /sp volume <0-100>"; }
    }
    if (sub == "mute")   return send({{"action","mute"}});
    if (sub == "unmute") return send({{"action","unmute"}});

    // ── SHUFFLE / REPEAT ──────────────────────────────────────────────────────
    if (sub == "shuffle") {
        if (args == "on")  return send({{"action","shuffle_on"}});
        if (args == "off") return send({{"action","shuffle_off"}});
        return "\u274c Usage: /sp shuffle on|off";
    }
    if (sub == "repeat") {
        if (args == "off")  return send({{"action","repeat_off"}});
        if (args == "all")  return send({{"action","repeat_all"}});
        if (args == "one")  return send({{"action","repeat_one"}});
        return "\u274c Usage: /sp repeat off|all|one";
    }

    // ── HEART ─────────────────────────────────────────────────────────────────
    if (sub == "heart")   return send({{"action","heart"}});
    if (sub == "unheart") return send({{"action","unheart"}});

    // ── ADVANCED ──────────────────────────────────────────────────────────────
    if (sub == "queue")    return args.empty() ? "\u274c Usage: /sp queue <song>" : send({{"action","queue"},{"query",args}});
    if (sub == "playlist") return args.empty() ? "\u274c Usage: /sp playlist <name>" : send({{"action","playlist"},{"query",args}});
    if (sub == "radio")    return args.empty() ? "\u274c Usage: /sp radio <artist>" : send({{"action","radio"},{"query",args}});

    // ── STATUS ────────────────────────────────────────────────────────────────
    if (sub == "status" || sub == "now") {
        if (!SpotifyWS::instance().is_client_connected())
            return not_connected();

        auto s = SpotifyStateManager::instance().get();
        if (s.title.empty()) return "\u274c No track playing.";

        std::string repeat_str = s.repeat_mode == 0 ? "OFF" : s.repeat_mode == 1 ? "ALL" : "ONE";
        return
            "\ud83c\udfb5 " + s.title + "\n"
            "\ud83d\udc64 " + s.artist + "\n"
            "\ud83d\udcc0 " + s.album  + "\n"
            "\u23f1 "  + ms_to_time(s.progress_ms) + " / " + ms_to_time(s.duration_ms) + "\n"
            "\ud83d\udd0a " + std::to_string(s.volume) + "%\n"
            "\ud83d\udd00 " + std::string(s.shuffle ? "ON" : "OFF") + "\n"
            "\ud83d\udd01 " + repeat_str + "\n"
            "\u2764\ufe0f "  + std::string(s.hearted ? "YES" : "NO");
    }

    return "\u274c Unknown /sp command. Try /sp status, /sp play <song>, /sp pause, /sp next, etc.";
}

} // namespace sara
