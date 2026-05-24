#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>

namespace sara {

class SpotifyDock {
public:
    static SpotifyDock& instance();

    // Called by /sp dock command
    void send_dock(const std::string& chat_id);

    // Called by DockRouter for button callbacks
    bool handle_callback(const std::string& chat_id,
                         const std::string& callback_data,
                         const std::string& callback_query_id,
                         int message_id);

    void stop();

private:
    SpotifyDock()  = default;
    ~SpotifyDock() = default;

    void update_loop();
    std::string render_text();
    std::string render_progress_bar(int progress_ms, int duration_ms);

    struct DockSession {
        std::string chat_id;
        int         message_id = 0;
    };

    std::mutex                     mtx_;
    std::map<std::string, DockSession> sessions_; // chat_id -> session

    std::atomic<bool>  running_{false};
    std::thread        update_thread_;
};

} // namespace sara
