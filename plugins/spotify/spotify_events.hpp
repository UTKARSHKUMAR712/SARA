#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include "spotify_state.hpp"

namespace sara {

enum class SpotifyEventType {
    SongChanged,
    Paused,
    Resumed,
    VolumeChanged,
    ShuffleChanged,
    RepeatChanged,
    HeartChanged,
    Connected,
    Disconnected,
    MetadataUpdate,
};

using SpotifyEventCallback = std::function<void(SpotifyEventType, const SpotifyState&)>;

class SpotifyEvents {
public:
    static SpotifyEvents& instance();

    void subscribe(SpotifyEventCallback cb);
    void emit(SpotifyEventType type, const SpotifyState& state);

private:
    std::mutex mtx_;
    std::vector<SpotifyEventCallback> subscribers_;
};

} // namespace sara
