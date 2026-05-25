#pragma once
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>

namespace sara {

struct RateLimitContext {
    int count = 0;
    std::chrono::steady_clock::time_point window_start;
};

struct TrustedUser {
    long long user_id    = 0;
    std::string username;       // @handle
    std::string first_name;
    std::string last_name;
    std::string phone;          // rarely available via bot API
    std::string added_at;       // ISO-8601 timestamp
    bool blocked             = false;
    bool session_active      = false;
};

class SecurityManager {
public:
    static SecurityManager& instance();

    void initialize(const std::string& data_dir);

    // Check if user is in trusted_users.json and NOT blocked
    bool is_trusted_user(long long user_id);

    // Block / un-block a trusted user
    bool block_user(long long user_id);
    bool unblock_user(long long user_id);

    // Check if user has an active authenticated session
    bool is_session_authenticated(long long user_id);

    // Auth flows
    bool authenticate_session(long long user_id, const std::string& session_password);

    // root_message carries Telegram "from" JSON so we can store rich details
    bool authenticate_root(long long user_id, const std::string& root_password,
                           const std::string& username   = "",
                           const std::string& first_name = "",
                           const std::string& last_name  = "");

    // Generates a root password and shows a Windows notification
    bool trigger_root_auth_flow(long long user_id);

    std::string get_latest_root_password();

    // Snapshot of all trusted users (for GUI)
    std::vector<TrustedUser> get_trusted_users();

    void logout(long long user_id);

    // Enforce 5 messages/sec rate limit (returns false if exceeding limit)
    bool check_rate_limit(long long user_id);

private:
    SecurityManager() = default;

    void load_trusted_users();
    void save_trusted_users();
    std::string generate_crypto_password(int length = 11);
    std::string current_timestamp();

    std::string trusted_users_file_;

    std::unordered_map<long long, TrustedUser> trusted_users_;   // keyed by user_id
    std::unordered_set<long long> authenticated_sessions_;
    std::unordered_map<long long, std::string> pending_root_passwords_;
    std::string latest_root_password_;

    std::unordered_map<long long, RateLimitContext> rate_limits_;
    std::mutex mtx_;
};

} // namespace sara
