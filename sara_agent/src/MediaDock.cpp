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

MediaDock& MediaDock::instance() {
    static MediaDock s_instance;
    return s_instance;
}

MediaDock::MediaDock() {
    // MediaService::init() is called in AgentInitializer before this
    Logger::instance().info("[MediaDock] Initializing singleton");

    // Register event callback from GSMTC — fires on any state change
    media::MediaService::instance().set_global_update_callback([this](const media::MediaSessionInfo& info) {
        Logger::instance().info("[MediaDock] GSMTC event: " + info.title + " | " + info.playback_status);
        std::vector<std::pair<std::string, int>> active;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto const& pair : m_active_docks) {
                if (pair.second.message_id != -1) {
                    active.push_back({pair.first, pair.second.message_id});
                }
            }
        }
        for (auto const& p : active) {
            send_update(p.first, p.second);
        }
    });

    // Periodic fallback update (every 5s) for timeline slider
    std::thread([this]() { update_loop(); }).detach();
    Logger::instance().info("[MediaDock] Initialized successfully");
}

MediaDock::~MediaDock() {
    m_running = false;
}

static std::string format_time(int64_t ms) {
    if (ms <= 0) return "00:00";
    int total_seconds = (int)(ms / 1000);
    int m = total_seconds / 60;
    int s = total_seconds % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
    return std::string(buf);
}

void MediaDock::send_update(const std::string& chat_id, int message_id) {
    Logger::instance().info("[MediaDock] send_update for chat=" + chat_id + " msg_id=" + std::to_string(message_id));

    auto info = media::MediaService::instance().get_active_session();
    Logger::instance().info("[MediaDock] MediaService returned: title='" + info.title + "' status='" + info.playback_status + "'");

    std::string text = "\xF0\x9F\x8E\xB5 **SARA Media Dock**\n\n";

    if (info.title.empty() && info.artist.empty() && info.source_app.empty()) {
        text += "\xE2\x9D\x8C No active media session detected.\n";
        text += "\xE2\x84\xB9\xEF\xB8\x8F Play something in Spotify, YouTube, or VLC first.";
    } else {
        if (!info.title.empty())       text += "\xF0\x9F\x8F\xB7 **Title:** " + info.title + "\n";
        if (!info.artist.empty())      text += "\xF0\x9F\x91\xA4 **Artist:** " + info.artist + "\n";
        if (!info.album.empty())       text += "\xF0\x9F\x92\xBF **Album:** " + info.album + "\n";
        if (!info.source_app.empty())  text += "\xF0\x9F\x93\xB1 **App:** " + info.source_app + "\n";
        text += "\xE2\x96\xB6\xEF\xB8\x8F **Status:** " + info.playback_status + "\n\n";

        if (info.duration_ms > 0) {
            int64_t pos_ms = info.position_ms;
            // Live-advance position if playing
            if (info.playback_status == "Playing" && info.last_updated_time_ms > 0) {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                // last_updated_time_ms is stored in ms (converted from 100ns ticks from 1601)
                // system_clock is from 1970 — offset ~11644473600 seconds = 11644473600000 ms
                int64_t epoch_offset_ms = 11644473600000LL;
                int64_t lut_unix_ms = info.last_updated_time_ms - epoch_offset_ms;
                int64_t elapsed = now_ms - lut_unix_ms;
                if (elapsed > 0 && elapsed < 30000) {
                    pos_ms += elapsed;
                }
            }
            if (pos_ms > info.duration_ms) pos_ms = info.duration_ms;

            text += "`" + format_time(pos_ms) + " / " + format_time(info.duration_ms) + "`\n";

            // Progress bar (15 slots)
            int progress = (int)((double)pos_ms / info.duration_ms * 15);
            if (progress < 0) progress = 0;
            if (progress > 15) progress = 15;
            std::string bar = "";
            for (int i = 0; i < 15; i++) bar += (i == progress) ? "\xF0\x9F\x94\x98" : "\xE2\x96\xAC";
            text += bar + "\n";
        }

        if (!info.genres.empty()) {
            text += "\xF0\x9F\x8E\xB8 **Genres:** ";
            for (size_t i = 0; i < info.genres.size(); ++i) {
                if (i > 0) text += ", ";
                text += info.genres[i];
            }
            text += "\n";
        }
    }

    // Build inline keyboard using capability flags
    bool can_prev  = info.capabilities.can_previous || !info.title.empty();
    bool can_next  = info.capabilities.can_next      || !info.title.empty();
    bool is_playing = info.playback_status == "Playing";

    json kb = json::array({
        json::array({
            {{"text", "\xE2\x8F\xAE\xEF\xB8\x8F"}, {"callback_data", "dock_media:prev"}},
            {{"text", is_playing ? "\xE2\x8F\xB8\xEF\xB8\x8F Pause" : "\xE2\x96\xB6\xEF\xB8\x8F Play"}, {"callback_data", "dock_media:play_pause"}},
            {{"text", "\xE2\x8F\xAD\xEF\xB8\x8F"}, {"callback_data", "dock_media:next"}}
        }),
        json::array({
            {{"text", "\xF0\x9F\x94\x84 Refresh"}, {"callback_data", "dock_media:refresh"}}
        })
    });

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_docks.count(chat_id)) return;

    auto& state = m_active_docks[chat_id];

    if (message_id == -1) {
        // New message
        Logger::instance().info("[MediaDock] Sending NEW media dock message");
        bool sent = g_telegram.send_inline_keyboard(chat_id, text, kb);
        Logger::instance().info("[MediaDock] send_inline_keyboard result: " + std::string(sent ? "OK" : "FAILED"));
        // We'll get the message_id on next refresh via state tracking
        // For now mark as sent with placeholder
        state.last_text = text;
        state.last_kb = kb.dump();
    } else {
        // Edit existing
        if (state.last_text != text || state.last_kb != kb.dump()) {
            Logger::instance().info("[MediaDock] Editing media dock message " + std::to_string(message_id));
            bool ok = g_telegram.edit_message_text(chat_id, message_id, text, kb);
            if (ok) {
                state.last_text = text;
                state.last_kb = kb.dump();
            } else {
                Logger::instance().info("[MediaDock] edit failed — removing tracking for chat " + chat_id);
                m_active_docks.erase(chat_id);
            }
        }
    }
}

void MediaDock::send_dock(const std::string& chat_id) {
    Logger::instance().info("[MediaDock] send_dock called for " + chat_id);

    // Register dock tracking
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active_docks[chat_id] = DockState{};
    }

    // Send initial message — get message_id for live updates
    auto info = media::MediaService::instance().get_active_session();
    Logger::instance().info("[MediaDock] Initial session: title='" + info.title + "' status='" + info.playback_status + "'");

    std::string text = "\xF0\x9F\x8E\xB5 **SARA Media Dock**\n\n";
    if (info.title.empty() && info.artist.empty()) {
        text += "\xE2\x9D\x8C No active media session detected.\n";
        text += "\xE2\x84\xB9\xEF\xB8\x8F Play something in Spotify, YouTube, or VLC first.";
    } else {
        if (!info.title.empty())  text += "\xF0\x9F\x8F\xB7 **Title:** " + info.title + "\n";
        if (!info.artist.empty()) text += "\xF0\x9F\x91\xA4 **Artist:** " + info.artist + "\n";
        if (!info.source_app.empty()) text += "\xF0\x9F\x93\xB1 **App:** " + info.source_app + "\n";
        text += "\xE2\x96\xB6\xEF\xB8\x8F **Status:** " + info.playback_status + "\n";
        if (info.duration_ms > 0) {
            text += "`" + format_time(info.position_ms) + " / " + format_time(info.duration_ms) + "`\n";
        }
    }

    bool is_playing = info.playback_status == "Playing";
    json kb = json::array({
        json::array({
            {{"text", "\xE2\x8F\xAE\xEF\xB8\x8F"}, {"callback_data", "dock_media:prev"}},
            {{"text", is_playing ? "\xE2\x8F\xB8\xEF\xB8\x8F Pause" : "\xE2\x96\xB6\xEF\xB8\x8F Play"}, {"callback_data", "dock_media:play_pause"}},
            {{"text", "\xE2\x8F\xAD\xEF\xB8\x8F"}, {"callback_data", "dock_media:next"}}
        }),
        json::array({
            {{"text", "\xF0\x9F\x94\x84 Refresh"}, {"callback_data", "dock_media:refresh"}}
        })
    });

    Logger::instance().info("[MediaDock] Calling send_inline_keyboard");
    bool sent = g_telegram.send_inline_keyboard(chat_id, text, kb);
    Logger::instance().info("[MediaDock] send_inline_keyboard result: " + std::string(sent ? "OK" : "FAILED"));

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_active_docks.count(chat_id)) {
            m_active_docks[chat_id].last_text = text;
            m_active_docks[chat_id].last_kb = kb.dump();
            // Note: message_id stays -1 because send_inline_keyboard returns bool not int
            // We'll leave live-update via event callbacks only
        }
    }
}

void MediaDock::handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id) {
    Logger::instance().info("[MediaDock] handle_action: " + action);

    auto& svc = media::MediaService::instance();
    if (action == "play_pause") {
        svc.toggle_play_pause();
        Logger::instance().info("[MediaDock] Sent toggle_play_pause");
    } else if (action == "next") {
        svc.next();
        Logger::instance().info("[MediaDock] Sent next");
    } else if (action == "prev") {
        svc.previous();
        Logger::instance().info("[MediaDock] Sent previous");
    } else if (action == "refresh") {
        Logger::instance().info("[MediaDock] Manual refresh requested");
    }

    // Acknowledge button press immediately
    g_telegram.answer_callback_query(callback_query_id, "Done");

    // Give GSMTC 300ms to process the command, then refresh dock
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    send_update(chat_id, message_id);
}

void MediaDock::update_loop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::vector<std::pair<std::string, int>> active;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto const& pair : m_active_docks) {
                if (pair.second.message_id != -1) {
                    active.push_back({pair.first, pair.second.message_id});
                }
            }
        }
        for (auto const& p : active) {
            send_update(p.first, p.second);
        }
    }
}

} // namespace sara
