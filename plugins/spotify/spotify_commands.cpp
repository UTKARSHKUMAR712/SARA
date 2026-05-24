#include "spotify_commands.hpp"
#include "spotify_ws.hpp"
#include "spotify_state.hpp"
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <shellapi.h>

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

    // ── HELP ──────────────────────────────────────────────────────────────────
    if (sub == "help") {
        return "\xF0\x9F\x8E\xB5 *SARA Spotify Plugin Commands*\n\n"
               "*Dock Control:*\n"
               "• `/sp dock` — Launch interactive control panel (live updating)\n\n"
               "*Playback Controls:*\n"
               "• `/sp play <song>` — Play a song, playlist, or artist\n"
               "• `/sp pause` — Pause music\n"
               "• `/sp resume` — Resume music\n"
               "• `/sp toggle` — Play / Pause toggle\n"
               "• `/sp next` — Skip to the next track\n"
               "• `/sp prev` — Go back to the previous track\n\n"
               "*Progress & Volume:*\n"
               "• `/sp seek <sec>` — Seek to specific seconds\n"
               "• `/sp forward [sec]` — Skip forward (default 10s)\n"
               "• `/sp backward [sec]` — Skip backward (default 10s)\n"
               "• `/sp vol <0-100>` — Set volume level\n"
               "• `/sp mute` / `/sp unmute`\n\n"
               "*Modes & Customization:*\n"
               "• `/sp shuffle on|off` — Toggle shuffle mode\n"
               "• `/sp repeat off|all|one` — Set repeat mode\n"
               "• `/sp heart` / `/sp unheart` — Like/unlike current song\n\n"
               "*App Control:*\n"
               "• `/sp open` — Open the Spotify desktop application\n\n"
               "*Advanced:* \n"
               "• `/sp queue <song>` — Queue up a song\n"
               "• `/sp playlist <name>` — Play one of your playlists\n"
               "• `/sp radio <artist>` — Queue up radio for an artist\n"
               "• `/sp status` — Get detailed current track status";
    }

    // ── OPEN ──────────────────────────────────────────────────────────────────
    if (sub == "open" || sub == "start" || sub == "launch") {
        HINSTANCE result = ShellExecuteA(nullptr, "open", "spotify:", nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32) {
            return "\u274c Failed to open Spotify.";
        }
        return "\u2705 Opening Spotify...";
    }

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
            "\xF0\x9F\x8E\xB5 " + s.title + "\n"
            "\xF0\x9F\x91\xA4 " + s.artist + "\n"
            "\xF0\x9F\x93\x80 " + s.album  + "\n"
            "\xE2\x8F\xB1 " + ms_to_time(s.progress_ms) + " / " + ms_to_time(s.duration_ms) + "\n"
            "\xF0\x9F\x94\x8A " + std::to_string(s.volume) + "%\n"
            "\xF0\x9F\x94\x80 " + std::string(s.shuffle ? "ON" : "OFF") + "\n"
            "\xF0\x9F\x94\x81 " + repeat_str + "\n"
            "\xE2\x9D\xA4\xEF\xB8\x8F " + std::string(s.hearted ? "YES" : "NO");
    }

    return "\u274c Unknown /sp command. Try /sp status, /sp play <song>, /sp pause, /sp next, etc.";
}

} // namespace sara
