#pragma once

#include <string>
#include <map>
#include <mutex>

namespace sara {

class MediaDock {
public:
    static MediaDock& instance();

    void send_dock(const std::string& chat_id);
    void handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id);

private:
    MediaDock();
    ~MediaDock();

    void update_loop();
    void send_update(const std::string& chat_id, int message_id);

    struct DockState {
        int message_id = -1;
        std::string last_text;
        std::string last_kb;
    };

    std::map<std::string, DockState> m_active_docks;
    std::mutex m_mutex;
    bool m_running = true;
};

} // namespace sara
