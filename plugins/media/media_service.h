#pragma once

#include "media_types.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>

#include <functional>
#include <map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace sara {
namespace media {

// ─────────────────────────────────────────────────────────────────────────────
// MediaSessionWrapper — wraps a single GSMTC session
// Handles events: MediaPropertiesChanged, PlaybackInfoChanged, TimelinePropertiesChanged
// ─────────────────────────────────────────────────────────────────────────────

class MediaSessionWrapper : public std::enable_shared_from_this<MediaSessionWrapper> {
public:
    explicit MediaSessionWrapper(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session);
    ~MediaSessionWrapper();

    void init();
    MediaSessionInfo get_info();
    void set_update_callback(std::function<void(const MediaSessionInfo&)> callback);

    void play();
    void pause();
    void toggle_play_pause();
    void next();
    void previous();

    // Called by MediaService's refresh timer thread (must be public)
    void refresh_playback_and_timeline();

private:
    winrt::fire_and_forget refresh_all();

    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession m_session{nullptr};

    winrt::event_token m_properties_token;
    winrt::event_token m_playback_token;
    winrt::event_token m_timeline_token;

    MediaSessionInfo m_info;
    std::mutex m_mutex;
    std::function<void(const MediaSessionInfo&)> m_callback;
};

// ─────────────────────────────────────────────────────────────────────────────
// MediaService — manages all GSMTC sessions
// Fully UI-agnostic: No Telegram or UI logic here.
// ─────────────────────────────────────────────────────────────────────────────

class MediaService {
public:
    static MediaService& instance();

    winrt::fire_and_forget init();

    MediaSessionInfo              get_active_session();
    std::vector<MediaSessionInfo> get_all_sessions();

    void play           (const std::string& session_id = "");
    void pause          (const std::string& session_id = "");
    void toggle_play_pause(const std::string& session_id = "");
    void next           (const std::string& session_id = "");
    void previous       (const std::string& session_id = "");

    // Start a dedicated refresh thread (call once after init())
    void start_refresh_timer();

    void set_global_update_callback(std::function<void(const MediaSessionInfo&)> callback);

private:
    MediaService() = default;
    ~MediaService();

    void update_sessions();

    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager m_manager{nullptr};
    winrt::event_token m_sessions_changed_token;
    winrt::event_token m_current_session_changed_token;

    std::map<std::string, std::shared_ptr<MediaSessionWrapper>> m_sessions;
    std::string m_active_session_id;
    std::mutex m_mutex;
    std::function<void(const MediaSessionInfo&)> m_global_callback;
};

} // namespace media
} // namespace sara
