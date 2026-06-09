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

// ─── Emoji UTF-8 byte constants ──────────────────────────────────────────────
static const std::string E_MUSIC   = "\xF0\x9F\x8E\xB5";
static const std::string E_LABEL   = "\xF0\x9F\x8F\xB7";
static const std::string E_PERSON  = "\xF0\x9F\x91\xA4";
static const std::string E_DISC    = "\xF0\x9F\x92\xBF";
static const std::string E_PLAY    = "\xE2\x96\xB6\xEF\xB8\x8F";
static const std::string E_PAUSE   = "\xE2\x8F\xB8\xEF\xB8\x8F";
static const std::string E_STOP    = "\xE2\x8F\xB9\xEF\xB8\x8F";
static const std::string E_PREV    = "\xE2\x8F\xAE\xEF\xB8\x8F";
static const std::string E_NEXT    = "\xE2\x8F\xAD\xEF\xB8\x8F";
static const std::string E_REFRESH = "\xF0\x9F\x94\x84";
static const std::string E_BULLET  = "\xF0\x9F\x94\x98";
static const std::string E_BAR     = "\xE2\x96\xAC";
static const std::string E_NOTE    = "\xF0\x9F\x8E\xB8";
static const std::string E_CROSS   = "\xE2\x9D\x8C";
static const std::string E_INFO    = "\xE2\x84\xB9\xEF\xB8\x8F";
static const std::string E_DASH    = "\xE2\x80\x94";

// ─── Short session hash for callback_data (Telegram limit: 64 bytes) ─────────
// FNV-1a 32-bit → 8-char hex. "dock_media:play_pause:xxxxxxxx" = 31 chars, safe.
static std::string short_sid(const std::string& session_id) {
    uint32_t h = 2166136261u;
    for (unsigned char c : session_id) { h ^= c; h *= 16777619u; }
    char buf[9]; snprintf(buf, sizeof(buf), "%08x", h);
    return std::string(buf);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string fmt_time(int64_t ms) {
    if (ms <= 0) return "00:00";
    int s = (int)(ms / 1000);
    char buf[16]; snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}

static json build_keyboard(const media::MediaSessionInfo& info) {
    bool playing = (info.playback_status == "Playing");
    std::string sh = short_sid(info.session_id); // safe short hash
    return json::array({
        json::array({
            {{"text", E_PREV + " Prev"},
             {"callback_data", "dock_media:prev:" + sh}},
            {{"text", playing ? E_PAUSE + " Pause" : E_PLAY + " Play"},
             {"callback_data", "dock_media:play_pause:" + sh}},
            {{"text", "Next " + E_NEXT},
             {"callback_data", "dock_media:next:" + sh}}
        }),
        json::array({
            {{"text", E_REFRESH + " Refresh"},
             {"callback_data", "dock_media:refresh:" + sh}}
        })
    });
}

static std::string build_text(const media::MediaSessionInfo& info) {
    std::string text = E_MUSIC + " **SARA Media** " + E_DASH + " " + info.source_app + "\n\n";

    if (!info.title.empty())  text += E_LABEL  + " **" + info.title  + "**\n";
    if (!info.artist.empty()) text += E_PERSON + " " + info.artist + "\n";
    if (!info.album.empty())  text += E_DISC   + " " + info.album  + "\n";
    text += "\n";

    std::string sicon = (info.playback_status == "Playing") ? E_PLAY :
                        (info.playback_status == "Paused")  ? E_PAUSE : E_STOP;
    text += sicon + " " + info.playback_status;

    if (info.duration_ms > 0) {
        // Estimate live position: pos = stored_pos + time_elapsed_since_last_update
        int64_t pos = info.position_ms;
        if (info.playback_status == "Playing" && info.last_updated_time_ms > 0) {
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            // GSMTC epoch = Windows FILETIME (1601), Unix offset = 11644473600000 ms
            int64_t lut_unix = info.last_updated_time_ms - 11644473600000LL;
            int64_t elapsed  = now_ms - lut_unix;
            if (elapsed > 0 && elapsed < 3600000LL) {
                pos += elapsed;
            }
            if (pos > info.duration_ms) pos = info.duration_ms;
        }
        text += "  `" + fmt_time(pos) + " / " + fmt_time(info.duration_ms) + "`\n";

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

    // GSMTC event callback — only fires if no action is in progress
    media::MediaService::instance().set_global_update_callback(
        [this](const media::MediaSessionInfo& info) {
            Logger::instance().info("[MediaDock] GSMTC event: "
                + info.title + " [" + info.playback_status + "]");

            // Skip event refresh if a button action is in progress (prevents race condition)
            auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            if ((now - m_last_action_ns.load()) < 1200000000LL) { // 1.2 seconds
                Logger::instance().info("[MediaDock] Event refresh skipped — action in progress");
                return;
            }

            std::vector<std::pair<std::string, int>> to_refresh;
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                for (auto& [cid, smap] : m_docks) {
                    auto it = smap.find(info.session_id);
                    if (it != smap.end() && it->second.message_id > 0)
                        to_refresh.push_back({cid, it->second.message_id});
                }
            }
            for (auto& [cid, mid] : to_refresh)
                do_edit(cid, info.session_id, mid, false);
        }
    );

    // 5-second fallback loop — always refreshes, no skip-if-same
    std::thread([this]() { update_loop(); }).detach();
    Logger::instance().info("[MediaDock] Initialized OK");
}

MediaDock::~MediaDock() { m_running = false; }

// ─── Internal edit — makes the actual HTTP call ───────────────────────────────
// force=true skips the content-equality check (used by 5s loop and button refresh)

void MediaDock::do_edit(const std::string& chat_id,
                        const std::string& session_id,
                        int message_id,
                        bool force)
{
    // Get fresh info (get_info now reads timeline synchronously)
    auto all = media::MediaService::instance().get_all_sessions();
    media::MediaSessionInfo info;
    bool found = false;
    for (auto& s : all) {
        if (s.session_id == session_id) { info = s; found = true; break; }
    }
    if (!found) {
        Logger::instance().info("[MediaDock] Session not found: " + session_id);
        return;
    }

    std::string nt  = build_text(info);
    json        nk  = build_keyboard(info);
    std::string nks = nk.dump();

    if (!force) {
        // Skip if content unchanged (for event-driven path only)
        std::lock_guard<std::mutex> lk(m_mutex);
        auto ci = m_docks.find(chat_id);
        if (ci == m_docks.end()) return;
        auto si = ci->second.find(session_id);
        if (si == ci->second.end()) return;
        if (si->second.last_text == nt && si->second.last_kb == nks) return;
    }

    // HTTP call — NO mutex held
    Logger::instance().info("[MediaDock] Editing msg=" + std::to_string(message_id)
                            + " force=" + (force ? "yes" : "no"));
    bool ok = g_telegram.edit_message_text(chat_id, message_id, nt, nk);

    std::lock_guard<std::mutex> lk(m_mutex);
    if (ok) {
        auto& d = m_docks[chat_id][session_id];
        d.last_text = nt;
        d.last_kb   = nks;
    } else {
        // Message deleted or Telegram error — remove tracking
        auto ci = m_docks.find(chat_id);
        if (ci != m_docks.end()) ci->second.erase(session_id);
    }
}

// ─── send_dock ───────────────────────────────────────────────────────────────

void MediaDock::send_dock(const std::string& chat_id) {
    Logger::instance().info("[MediaDock] send_dock chat=" + chat_id);

    // Clear old tracking for this chat
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_docks.erase(chat_id);
    }

    auto sessions = media::MediaService::instance().get_all_sessions();
    Logger::instance().info("[MediaDock] Sessions: " + std::to_string(sessions.size()));

    if (sessions.empty()) {
        json kb = json::array({
            json::array({{{"text", E_REFRESH + " Refresh"}, {"callback_data", "dock_media:refresh:"}}})
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

    for (auto& info : sessions) {
        Logger::instance().info("[MediaDock] Sending: " + info.title
                                + " [" + info.source_app + "] sid=" + info.session_id);
        std::string t = build_text(info);
        json        k = build_keyboard(info);

        int mid = g_telegram.send_with_keyboard(chat_id, t, k);
        Logger::instance().info("[MediaDock] msg_id=" + std::to_string(mid));

        if (mid > 0) {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto& d    = m_docks[chat_id][info.session_id];
            d.message_id = mid;
            d.last_text  = t;
            d.last_kb    = k.dump();

            // Also store the short_sid → session_id mapping
            m_hash_to_sid[short_sid(info.session_id)] = info.session_id;
        }
    }
}

// ─── resolve_session_id — maps short hash back to full session_id ─────────────

std::string MediaDock::resolve_sid(const std::string& hash_or_sid,
                                   const std::string& chat_id)
{
    // Try direct match first
    auto all = media::MediaService::instance().get_all_sessions();
    for (auto& s : all) if (s.session_id == hash_or_sid) return hash_or_sid;

    // Look up in hash map
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_hash_to_sid.find(hash_or_sid);
        if (it != m_hash_to_sid.end()) return it->second;
    }

    // Fall back: check all tracked sessions for this chat
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto ci = m_docks.find(chat_id);
        if (ci != m_docks.end() && !ci->second.empty()) {
            return ci->second.begin()->first; // first tracked session
        }
    }

    // Last resort: active session
    return media::MediaService::instance().get_active_session().session_id;
}

// ─── handle_action ───────────────────────────────────────────────────────────

void MediaDock::handle_action(const std::string& chat_id, const std::string& action,
                              const std::string& callback_query_id, int message_id,
                              const std::string& sid_hint)
{
    Logger::instance().info("[MediaDock] action=" + action + " hint=" + sid_hint);

    std::string session_id = resolve_sid(sid_hint, chat_id);
    Logger::instance().info("[MediaDock] Resolved session: " + session_id);

    // Mark action in progress — suppresses GSMTC event-based refreshes for 1.2s
    m_last_action_ns.store(
        std::chrono::steady_clock::now().time_since_epoch().count());

    auto& svc = media::MediaService::instance();
    if      (action == "play_pause") svc.toggle_play_pause(session_id);
    else if (action == "next")       svc.next(session_id);
    else if (action == "prev")       svc.previous(session_id);
    // "refresh" just falls through

    // Ack button immediately (stops Telegram spinner)
    g_telegram.answer_callback_query(callback_query_id, "");

    // Ensure session is tracked with this message_id
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto& d = m_docks[chat_id][session_id];
        if (d.message_id <= 0) d.message_id = message_id;
        m_hash_to_sid[sid_hint] = session_id;
    }

    // Wait for GSMTC to process (600ms), then do ONE authoritative refresh
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    do_edit(chat_id, session_id, message_id, true); // force=true, always updates

    // Clear action flag
    m_last_action_ns.store(0);
}

// ─── update_loop — 5-second forced refresh ───────────────────────────────────

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
            do_edit(cid, sid, mid, true); // force=true: always edit even if text seems same
    }
}

} // namespace sara
