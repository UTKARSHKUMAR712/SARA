#include "spotify_state.hpp"

namespace sara {

SpotifyStateManager& SpotifyStateManager::instance() {
    static SpotifyStateManager inst;
    return inst;
}

SpotifyState SpotifyStateManager::get() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_;
}

void SpotifyStateManager::set(const SpotifyState& s) {
    std::lock_guard<std::mutex> lk(mtx_);
    state_ = s;
}

void SpotifyStateManager::set_connected(bool v) {
    std::lock_guard<std::mutex> lk(mtx_);
    state_.connected = v;
}

bool SpotifyStateManager::is_connected() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return state_.connected;
}

} // namespace sara
