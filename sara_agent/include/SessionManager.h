#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct Session {
    std::string session_id;
    std::string platform;   // "telegram", "gui", "cli"
    std::string chat_id;    // user/channel id
    long long   created_at;
    long long   last_activity;

    // Ephemeral state
    std::vector<json>        recent_commands;
    std::vector<std::string> last_targets;

    // Chat history
    struct Message {
        std::string role;
        std::string content;
    };
    std::vector<Message> history;
};

class SessionManager {
public:
    Session& get_or_create(const std::string& platform, const std::string& chat_id);
    Session* get(const std::string& session_id);
    void update_activity(const std::string& session_id);
    void add_command(const std::string& session_id, const json& command);
    
    // History
    void add_history(const std::string& session_id, const std::string& role, const std::string& content);

    // Target context
    void        set_last_target(const std::string& session_id, const std::string& target);
    std::string get_last_target(const std::string& session_id);
    void cleanup_expired(int max_idle_minutes = 30);
    json get_context(const std::string& session_id);

private:
    std::string generate_session_id();
    std::unordered_map<std::string, Session> sessions_;
    std::mutex mutex_;
};

}
