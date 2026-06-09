#pragma once

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>
#include <nlohmann/json.hpp>

namespace sara {

class MediaDock {
public:
    static MediaDock& instance();

    void send_dock(const std::string& chat_id);

    void handle_action(const std::string& chat_id, const std::string& action,
                       const std::string& callback_query_id, int message_id,
                       const std::string& sid_hint = "");

private:
    MediaDock();
    ~MediaDock();

    // Core edit — force=true always edits (5s loop / button press)
    //             force=false skips if content unchanged (GSMTC event path)
    void do_edit(const std::string& chat_id, const std::string& session_id,
                 int message_id, bool force);

    // Resolve short_sid hash or full session_id → full session_id
    std::string resolve_sid(const std::string& hash_or_sid, const std::string& chat_id);

    void update_loop();

    struct SessionDock {
        int         message_id = -1;
        std::string last_text;
        std::string last_kb;
    };

    // chat_id → session_id → dock state
    std::map<std::string, std::map<std::string, SessionDock>> m_docks;

    // short_sid hash → full session_id (for callback routing)
    std::map<std::string, std::string> m_hash_to_sid;

    std::mutex       m_mutex;
    std::atomic_bool m_running{true};

    // Timestamp of last button action (steady_clock ns) — suppresses event refresh for 1.2s
    std::atomic<long long> m_last_action_ns{0};
};

} // namespace sara
