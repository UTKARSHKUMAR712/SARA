#include "media_service.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <windows.h>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

namespace sara {
namespace media {

// ─────────────────────────────────────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────────────────────────────────────

static std::string to_utf8(const winrt::hstring& hstr) {
    if (hstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, hstr.c_str(), (int)hstr.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, hstr.c_str(), (int)hstr.size(), &out[0], size, nullptr, nullptr);
    return out;
}

static winrt::hstring utf8_to_hstring(const std::string& str) {
    if (str.empty()) return winrt::hstring{};
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return winrt::hstring{wstr};
}

// Simple MD5-less hash: FNV-1a on title+artist for caching purposes
static std::string make_cache_key(const std::string& title, const std::string& artist) {
    std::string data = title + "|" + artist;
    uint64_t hash = 14695981039346656037ULL;
    for (char c : data) {
        hash ^= (uint8_t)c;
        hash *= 1099511628211ULL;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)hash);
    return std::string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// MediaSessionWrapper
// ─────────────────────────────────────────────────────────────────────────────

MediaSessionWrapper::MediaSessionWrapper(GlobalSystemMediaTransportControlsSession session)
    : m_session(std::move(session))
{
    m_info.session_id = to_utf8(m_session.SourceAppUserModelId());

    // Try to build a friendly name from the App User Model ID
    // e.g. "Spotify.exe" -> "Spotify", "chrome.exe" -> "Chrome"
    std::string raw = m_info.session_id;
    // Strip path if present
    auto slash = raw.find_last_of("/\\");
    if (slash != std::string::npos) raw = raw.substr(slash + 1);
    // Strip .exe suffix
    if (raw.size() > 4 && raw.substr(raw.size() - 4) == ".exe") raw = raw.substr(0, raw.size() - 4);
    // Capitalize first letter
    if (!raw.empty()) raw[0] = (char)toupper((unsigned char)raw[0]);
    m_info.source_app = raw.empty() ? m_info.session_id : raw;
    m_info.source_exe = to_utf8(m_session.SourceAppUserModelId());
}

MediaSessionWrapper::~MediaSessionWrapper() {
    try {
        if (m_session) {
            m_session.MediaPropertiesChanged(std::exchange(m_properties_token, {}));
            m_session.PlaybackInfoChanged(std::exchange(m_playback_token, {}));
            m_session.TimelinePropertiesChanged(std::exchange(m_timeline_token, {}));
        }
    } catch (...) {}
}

void MediaSessionWrapper::init() {
    try {
        // GSMTC events pass (sender, args) — use lambdas to call our update method
        auto self = shared_from_this();
        m_properties_token = m_session.MediaPropertiesChanged([self](auto&&, auto&&) {
            self->refresh_all();
        });
        m_playback_token = m_session.PlaybackInfoChanged([self](auto&&, auto&&) {
            self->refresh_playback_and_timeline();
        });
        m_timeline_token = m_session.TimelinePropertiesChanged([self](auto&&, auto&&) {
            self->refresh_playback_and_timeline();
        });

        // Initial full refresh
        refresh_all();
    } catch (...) {}
}

MediaSessionInfo MediaSessionWrapper::get_info() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_info;
}

void MediaSessionWrapper::set_update_callback(std::function<void(const MediaSessionInfo&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

void MediaSessionWrapper::play()              { try { m_session.TryPlayAsync(); } catch (...) {} }
void MediaSessionWrapper::pause()             { try { m_session.TryPauseAsync(); } catch (...) {} }
void MediaSessionWrapper::toggle_play_pause() { try { m_session.TryTogglePlayPauseAsync(); } catch (...) {} }
void MediaSessionWrapper::next()              { try { m_session.TrySkipNextAsync(); } catch (...) {} }
void MediaSessionWrapper::previous()          { try { m_session.TrySkipPreviousAsync(); } catch (...) {} }

// ─── Private async helpers ─────────────────────────────────────────────────

winrt::fire_and_forget MediaSessionWrapper::refresh_all() {
    auto self = shared_from_this();
    try {
        auto props = co_await self->m_session.TryGetMediaPropertiesAsync();
        if (props) {
            std::unique_lock<std::mutex> lock(self->m_mutex);
            self->m_info.title        = to_utf8(props.Title());
            self->m_info.artist       = to_utf8(props.Artist());
            self->m_info.album        = to_utf8(props.AlbumTitle());
            self->m_info.album_artist = to_utf8(props.AlbumArtist());
            self->m_info.track_number = props.TrackNumber();
            self->m_info.genres.clear();
            for (auto const& g : props.Genres())
                self->m_info.genres.push_back(to_utf8(g));

            // Snapshot for thumbnail (must read outside lock)
            std::string title  = self->m_info.title;
            std::string artist = self->m_info.artist;
            auto thumbnail_ref = props.Thumbnail();
            lock.unlock();

            // Extract thumbnail asynchronously
            self->extract_thumbnail(thumbnail_ref, title, artist);
        }
        self->refresh_playback_and_timeline();
    } catch (...) {}
}

void MediaSessionWrapper::refresh_playback_and_timeline() {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto playback = m_session.GetPlaybackInfo();
        if (playback) {
            switch (playback.PlaybackStatus()) {
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:  m_info.playback_status = "Playing";  break;
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:   m_info.playback_status = "Paused";   break;
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:  m_info.playback_status = "Stopped";  break;
                case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed:   m_info.playback_status = "Closed";   break;
                default:                                                                m_info.playback_status = "Unknown";  break;
            }
            auto ctrl = playback.Controls();
            m_info.capabilities.can_play     = ctrl.IsPlayEnabled();
            m_info.capabilities.can_pause    = ctrl.IsPauseEnabled();
            m_info.capabilities.can_next     = ctrl.IsNextEnabled();
            m_info.capabilities.can_previous = ctrl.IsPreviousEnabled();
        }

        auto timeline = m_session.GetTimelineProperties();
        if (timeline) {
            m_info.position_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()).count();
            m_info.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime()).count();
            // LastUpdatedTime is a Windows::Foundation::DateTime (100-ns ticks from 1601)
            auto lut = timeline.LastUpdatedTime().time_since_epoch().count(); // 100ns ticks
            m_info.last_updated_time_ms = lut / 10000; // convert to ms
        }

        auto cb = m_callback;
        if (cb) cb(m_info);
    } catch (...) {}
}

winrt::fire_and_forget MediaSessionWrapper::extract_thumbnail(
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference thumbnail_ref,
    std::string title,
    std::string artist)
{
    auto self = shared_from_this();
    try {
        if (!thumbnail_ref) co_return;

        std::string key  = make_cache_key(title, artist);
        std::string dir  = "cache/media_thumbnails";
        std::string path = dir + "/" + key + ".jpg";

        // Check cache first
        if (std::filesystem::exists(path)) {
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_info.thumbnail_available = true;
            self->m_info.thumbnail_id   = key;
            self->m_info.thumbnail_path = path;
            co_return;
        }

        // Open stream asynchronously
        auto stream = co_await thumbnail_ref.OpenReadAsync();
        uint64_t size = stream.Size();
        if (size == 0) co_return;

        auto reader = Windows::Storage::Streams::DataReader(stream);
        co_await reader.LoadAsync((uint32_t)size);

        std::vector<uint8_t> buf(size);
        reader.ReadBytes(winrt::array_view<uint8_t>(buf.data(), buf.data() + size));

        std::filesystem::create_directories(dir);
        std::ofstream ofs(path, std::ios::binary);
        if (ofs) {
            ofs.write((char*)buf.data(), buf.size());
            std::lock_guard<std::mutex> lock(self->m_mutex);
            self->m_info.thumbnail_available = true;
            self->m_info.thumbnail_id   = key;
            self->m_info.thumbnail_path = path;
        }
    } catch (...) {}
}

// ─────────────────────────────────────────────────────────────────────────────
// MediaService
// ─────────────────────────────────────────────────────────────────────────────

MediaService& MediaService::instance() {
    static MediaService s_instance;
    return s_instance;
}

MediaService::~MediaService() {
    try {
        if (m_manager) {
            m_manager.SessionsChanged(std::exchange(m_sessions_changed_token, {}));
            m_manager.CurrentSessionChanged(std::exchange(m_current_session_changed_token, {}));
        }
    } catch (...) {}
}

winrt::fire_and_forget MediaService::init() {
    try {
        auto manager = co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_manager = manager;
        }
        if (manager) {
            m_sessions_changed_token = manager.SessionsChanged([this](auto&&, auto&&) {
                update_sessions();
            });
            m_current_session_changed_token = manager.CurrentSessionChanged([this](auto&&, auto&&) {
                update_sessions();
            });
            update_sessions();
        }
    } catch (...) {}
}

void MediaService::update_sessions() {
    GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        mgr = m_manager;
    }
    if (!mgr) return;

    try {
        auto current      = mgr.GetCurrentSession();
        auto sessions_vec = mgr.GetSessions();

        std::string new_active_id = current ? to_utf8(current.SourceAppUserModelId()) : "";
        std::vector<std::string> valid_ids;

        std::vector<std::pair<std::string, GlobalSystemMediaTransportControlsSession>> to_add;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_active_session_id = new_active_id;

            for (auto const& sess : sessions_vec) {
                std::string id = to_utf8(sess.SourceAppUserModelId());
                valid_ids.push_back(id);
                if (m_sessions.find(id) == m_sessions.end()) {
                    to_add.push_back({id, sess});
                }
            }

            // Remove dead sessions
            for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
                if (std::find(valid_ids.begin(), valid_ids.end(), it->first) == valid_ids.end())
                    it = m_sessions.erase(it);
                else
                    ++it;
            }
        }

        // Create and init new wrappers outside the lock (init may take time)
        for (auto& [id, sess] : to_add) {
            auto wrapper = std::make_shared<MediaSessionWrapper>(sess);
            wrapper->set_update_callback([this](const MediaSessionInfo& info) {
                std::function<void(const MediaSessionInfo&)> cb;
                {
                    std::lock_guard<std::mutex> cb_lock(m_mutex);
                    cb = m_global_callback;
                }
                if (cb) cb(info);
            });
            wrapper->init();
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sessions[id] = wrapper;
        }

    } catch (...) {}
}

// ─── Public API ─────────────────────────────────────────────────────────────

MediaSessionInfo MediaService::get_active_session() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(m_active_session_id);
    if (it != m_sessions.end()) return it->second->get_info();
    // Fallback: return first available session
    if (!m_sessions.empty()) return m_sessions.begin()->second->get_info();
    return MediaSessionInfo{};
}

std::vector<MediaSessionInfo> MediaService::get_all_sessions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MediaSessionInfo> list;
    for (auto const& [id, wrapper] : m_sessions)
        list.push_back(wrapper->get_info());
    return list;
}

void MediaService::set_global_update_callback(std::function<void(const MediaSessionInfo&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_global_callback = std::move(cb);
}

void MediaService::play(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string id = session_id.empty() ? m_active_session_id : session_id;
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) it->second->play();
}

void MediaService::pause(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string id = session_id.empty() ? m_active_session_id : session_id;
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) it->second->pause();
}

void MediaService::toggle_play_pause(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string id = session_id.empty() ? m_active_session_id : session_id;
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) it->second->toggle_play_pause();
}

void MediaService::next(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string id = session_id.empty() ? m_active_session_id : session_id;
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) it->second->next();
}

void MediaService::previous(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string id = session_id.empty() ? m_active_session_id : session_id;
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) it->second->previous();
}

} // namespace media
} // namespace sara
