#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace sara {
namespace media {

struct MediaCapabilities {
    bool can_play = false;
    bool can_pause = false;
    bool can_next = false;
    bool can_previous = false;
};

struct MediaSessionInfo {
    std::string session_id;
    std::string source_app;
    std::string source_exe;

    std::string title;
    std::string artist;
    std::string album;
    std::string album_artist;
    int track_number = 0;
    std::vector<std::string> genres;

    bool thumbnail_available = false;
    std::string thumbnail_id;
    std::string thumbnail_path;

    int64_t position_ms = 0;
    int64_t duration_ms = 0;
    int64_t last_updated_time_ms = 0;

    std::string playback_status = "Unknown"; // Playing, Paused, Stopped, etc.

    MediaCapabilities capabilities;
};

} // namespace media
} // namespace sara
