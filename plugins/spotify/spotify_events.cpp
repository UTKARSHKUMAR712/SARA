#include "spotify_events.hpp"

namespace sara {

SpotifyEvents& SpotifyEvents::instance() {
    static SpotifyEvents inst;
    return inst;
}

void SpotifyEvents::subscribe(SpotifyEventCallback cb) {
    std::lock_guard<std::mutex> lk(mtx_);
    subscribers_.push_back(std::move(cb));
}

void SpotifyEvents::emit(SpotifyEventType type, const SpotifyState& state) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& cb : subscribers_) cb(type, state);
}

} // namespace sara
