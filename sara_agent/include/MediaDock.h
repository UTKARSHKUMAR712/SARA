#pragma once

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace sara {

class MediaDock {
public:
    static MediaDock& instance();

    void send_dock(const std::string& chat_id);
    void handle_action(const std::string& chat_id, const std::string& action,
                       const std::string& callback_query_id, int message_id,
                       const std::string& session_id = "");

private:
    MediaDock();
    ~MediaDock();

    // Builds the text+keyboard for a given session, returns false if nothing to show
    bool build_dock_content(const std::string& session_id, std::string& out_text, nlohmann::json& out_kb);

    void update_loop();
    void refresh_session(const std::string& chat_id, const std::string& session_id, int message_id);

    // Per-chat, per-session state
    struct SessionDock {
        int         message_id  = -1;
        std::string last_text;
        std::string last_kb;
    };
    // key: chat_id -> session_id -> state
    std::map<std::string, std::map<std::string, SessionDock>> m_docks;

    std::mutex       m_mutex;
    std::atomic_bool m_running{true};
};

} // namespace sara
