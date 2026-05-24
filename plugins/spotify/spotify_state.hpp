#pragma once
#include <string>
#include <mutex>
#include <atomic>

namespace sara {

struct SpotifyState {
    std::string title;
    std::string artist;
    std::string album;
    int         duration_ms  = 0;
    int         progress_ms  = 0;
    int         volume       = 0;
    bool        playing      = false;
    bool        shuffle      = false;
    int         repeat_mode  = 0;   // 0=off 1=all 2=one
    bool        hearted      = false;
    bool        connected    = false;   // WS client connected?
};

class SpotifyStateManager {
public:
    static SpotifyStateManager& instance();

    SpotifyState get() const;
    void         set(const SpotifyState& s);
    void         set_connected(bool v);
    bool         is_connected() const;

private:
    mutable std::mutex mtx_;
    SpotifyState       state_;
};

} // namespace sara
