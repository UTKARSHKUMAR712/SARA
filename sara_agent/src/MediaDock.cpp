#include "../include/MediaDock.h"
#include "../include/TelegramGateway.h"
#include "../include/Logger.h"
#include "../../plugins/media/media_service.h"
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

using json = nlohmann::json;
extern sara::TelegramGateway g_telegram;

namespace sara {

// ─── Emoji as raw UTF-8 byte arrays (avoids unicode escape issues) ─────────
static const std::string E_MUSIC   = "\xF0\x9F\x8E\xB5";  // musical note
static const std::string E_LABEL   = "\xF0\x9F\x8F\xB7";  // label tag
static const std::string E_PERSON  = "\xF0\x9F\x91\xA4";  // person
static const std::string E_DISC    = "\xF0\x9F\x92\xBF";  // optical disc
static const std::string E_PLAY    = "\xE2\x96\xB6\xEF\xB8\x8F"; // play button
static const std::string E_PAUSE   = "\xE2\x8F\xB8\xEF\xB8\x8F"; // pause button
static const std::string E_STOP    = "\xE2\x8F\xB9\xEF\xB8\x8F"; // stop button
static const std::string E_PREV    = "\xE2\x8F\xAE\xEF\xB8\x8F"; // prev track
static const std::string E_NEXT    = "\xE2\x8F\xAD\xEF\xB8\x8F"; // next track
static const std::string E_REFRESH = "\xF0\x9F\x94\x84";  // counter-clockwise arrows
static const std::string E_BULLET  = "\xF0\x9F\x94\x98";  // red circle (progress dot)
static const std::string E_BAR     = "\xE2\x96\xAC";      // black rectangle (progress bar)
static const std::string E_NOTE    = "\xF0\x9F\x8E\xB8";  // guitar (genres)
static const std::string E_ART     = "\xF0\x9F\x96\xBC\xEF\xB8\x8F"; // framed picture
static const std::string E_CROSS   = "\xE2\x9D\x8C";      // red X
static const std::string E_INFO    = "\xE2\x84\xB9\xEF\xB8\x8F";     // info
static const std::string E_DASH    = "\xE2\x80\x94";       // em dash

// ─── Helpers ────────────────────────────────────────────────────────────────

static std::string fmt_time(int64_t ms) {
    if (ms <= 0) return "00:00";
    int s = (int)(ms / 1000);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}

static json build_keyboard(const media::MediaSessionInfo& info) {
    bool playing = (info.playback_status == "Playing");
    std::string sid = info.session_id;
    return json::array({
        json::array({
            {{"text", E_PREV + " Prev"},
             {"callback_data", "dock_media:prev:" + sid}},
            {{"text", (playing ? E_PAUSE + " Pause" : E_PLAY + " Play")},
             {"callback_data", "dock_media:play_pause:" + sid}},
            {{"text", "Next " + E_NEXT},
             {"callback_data", "dock_media:next:" + sid}}
        }),
        json::array({
            {{"text", E_REFRESH + " Refresh"},
             {"callback_data", "dock_media:refresh:" + sid}}
        })
    });
}

static std::string build_text(const media::MediaSessionInfo& info) {
    std::string text = E_MUSIC + " **SARA Media** " + E_DASH + " " + info.source_app + "\n\n";

    if (!info.title.empty())  text += E_LABEL  + " **" + info.title  + "**\n";
    if (!info.artist.empty()) text += E_PERSON + " " + info.artist + "\n";
    if (!info.album.empty())  text += E_DISC   + " " + info.album  + "\n";
    text += "\n";

    // Status icon + live time
    std::string sicon;
    if      (info.playback_status == "Playing") sicon = E_PLAY;
    else if (info.playback_status == "Paused")  sicon = E_PAUSE;
    else                                        sicon = E_STOP;
    text += sicon + " " + info.playback_status;

    if (info.duration_ms > 0) {
        int64_t pos = info.position_ms;
        // Live-advance if playing
        if (info.playback_status == "Playing" && info.last_updated_time_ms > 0) {
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            // GSMTC LastUpdatedTime: Windows FILETIME (1601 epoch)
            // Unix epoch offset = 11644473600000 ms
            int64_t lut_unix = info.last_updated_time_ms - 11644473600000LL;
            int64_t elapsed  = now_ms - lut_unix;
            if (elapsed > 0 && elapsed < 60000) pos += elapsed;
            if (pos > info.duration_ms) pos = info.duration_ms;
        }
        text += "  `" + fmt_time(pos) + " / " + fmt_time(info.duration_ms) + "`\n";

        // 20-slot progress bar
        int slot = (int)((double)pos / info.duration_ms * 20.0);
        if (slot < 0) slot = 0; if (slot > 20) slot = 20;
        std::string bar;
        for (int i = 0; i < 20; i++) bar += (i == slot) ? E_BULLET : E_BAR;
        text += bar + "\n";
    } else {
        text += "\n";
    }

    if (!info.genres.empty()) {
        text += E_NOTE + " ";
        for (size_t i = 0; i < info.genres.size(); ++i) {
            if (i) text += ", ";
            text += info.genres[i];
        }
        text += "\n";
    }
    if (info.thumbnail_available) text += E_ART + " _Artwork cached_\n";

    return text;
}

// ─────────────────────────────────────────────────────────────────────────────
// MediaDock Singleton
// ─────────────────────────────────────────────────────────────────────────────

MediaDock& MediaDock::instance() {
    static MediaDock s_instance;
    return s_instance;
}

MediaDock::MediaDock() {
    Logger::instance().info("[MediaDock] Initializing");

    // Register event-driven GSMTC callback
    media::MediaService::instance().set_global_update_callback(
        [this](const media::MediaSessionInfo& info) {
            Logger::instance().info("[MediaDock] GSMTC event: " + info.title
                + " [" + info.playback_status + "] sid=" + info.session_id);

            std::vector<std::pair<std::string, int>> to_update;
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                for (auto& [cid, smap] : m_docks) {
                    auto it = smap.find(info.session_id);
                    if (it != smap.end() && it->second.message_id > 0)
                        to_update.push_back({cid, it->second.message_id});
                }
            }
            for (auto& [cid, mid] : to_update)
                refresh_session(cid, info.session_id, mid);
        }
    );

    // 5-second fallback: refresh all tracked docks even without GSMTC events
    std::thread([this]() { update_loop(); }).detach();
    Logger::instance().info("[MediaDock] Initialized OK");
}

MediaDock::~MediaDock() {
    m_running = false;
}

// ─── refresh_session ─────────────────────────────────────────────────────────

void MediaDock::refresh_session(const std::string& chat_id,
                                const std::string& session_id,
                                int message_id)
{
    auto all = media::MediaService::instance().get_all_sessions();
    media::MediaSessionInfo info;
    bool found = false;
    for (auto& s : all) { if (s.session_id == session_id) { info = s; found = true; break; } }
    if (!found) return;

    std::string nt = build_text(info);
    json        nk = build_keyboard(info);
    std::string nks = nk.dump();

    // No-op if nothing changed
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto ci = m_docks.find(chat_id);
        if (ci == m_docks.end()) return;
        auto si = ci->second.find(session_id);
        if (si == ci->second.end()) return;
        if (si->second.last_text == nt && si->second.last_kb == nks) return;
    }

    // HTTP call — NO mutex held
    bool ok = g_telegram.edit_message_text(chat_id, message_id, nt, nk);
    Logger::instance().info("[MediaDock] edit msg=" + std::to_string(message_id)
                            + " ok=" + (ok ? "yes" : "no"));

    std::lock_guard<std::mutex> lk(m_mutex);
    if (ok) {
        auto& d = m_docks[chat_id][session_id];
        d.last_text = nt;
        d.last_kb   = nks;
    } else {
        // Message was deleted — remove tracking
        auto ci = m_docks.find(chat_id);
        if (ci != m_docks.end()) ci->second.erase(session_id);
    }
}

// ─── send_dock ───────────────────────────────────────────────────────────────

void MediaDock::send_dock(const std::string& chat_id) {
    Logger::instance().info("[MediaDock] send_dock chat=" + chat_id);

    auto sessions = media::MediaService::instance().get_all_sessions();
    Logger::instance().info("[MediaDock] Sessions: " + std::to_string(sessions.size()));

    if (sessions.empty()) {
        json kb = json::array({
            json::array({{{"text", E_REFRESH + " Refresh"},
                          {"callback_data", "dock_media:refresh:"}}})
        });
        int mid = g_telegram.send_with_keyboard(chat_id,
            E_MUSIC + " **SARA Media Dock**\n\n"
            + E_CROSS + " No active media session.\n"
            + E_INFO  + " Play something in Spotify, YouTube, or VLC.", kb);
        if (mid > 0) {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_docks[chat_id]["__empty__"].message_id = mid;
        }
        return;
    }

    // One dock per session
    for (auto& info : sessions) {
        Logger::instance().info("[MediaDock] Sending: " + info.title + " [" + info.source_app + "]");
        std::string t = build_text(info);
        json        k = build_keyboard(info);

        int mid = g_telegram.send_with_keyboard(chat_id, t, k);  // HTTP, no mutex
        Logger::instance().info("[MediaDock] msg_id=" + std::to_string(mid)
                                + " sid=" + info.session_id);

        if (mid > 0) {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto& d    = m_docks[chat_id][info.session_id];
            d.message_id = mid;
            d.last_text  = t;
            d.last_kb    = k.dump();
        }
    }
}

// ─── handle_action ───────────────────────────────────────────────────────────

void MediaDock::handle_action(const std::string& chat_id, const std::string& action,
                              const std::string& callback_query_id, int message_id,
                              const std::string& session_id)
{
    Logger::instance().info("[MediaDock] action=" + action + " sid=" + session_id);

    auto& svc = media::MediaService::instance();
    if      (action == "play_pause") svc.toggle_play_pause(session_id);
    else if (action == "next")       svc.next(session_id);
    else if (action == "prev")       svc.previous(session_id);

    // Ack immediately (stops Telegram spinner)
    g_telegram.answer_callback_query(callback_query_id, "");

    // Ensure session is tracked with this message_id
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto& d = m_docks[chat_id][session_id];
        if (d.message_id <= 0) d.message_id = message_id;
    }

    // Brief wait for GSMTC to process, then force refresh
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    refresh_session(chat_id, session_id, message_id);
}

// ─── update_loop ─────────────────────────────────────────────────────────────

void MediaDock::update_loop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::vector<std::tuple<std::string, std::string, int>> snap;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            for (auto& [cid, smap] : m_docks)
                for (auto& [sid, d] : smap)
                    if (d.message_id > 0 && sid != "__empty__")
                        snap.emplace_back(cid, sid, d.message_id);
        }

        Logger::instance().info("[MediaDock] 5s tick: " + std::to_string(snap.size()) + " docks");
        for (auto& [cid, sid, mid] : snap)
            refresh_session(cid, sid, mid);
    }
}

} // namespace sara
