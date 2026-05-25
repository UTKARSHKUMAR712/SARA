#include "../include/SecurityManager.h"
#include "../include/ConfigManager.h"
#include "../include/Logger.h"
#include <windows.h>
#include <fstream>
#include <random>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
extern sara::ConfigManager g_config;

namespace sara {

SecurityManager& SecurityManager::instance() {
    static SecurityManager inst;
    return inst;
}

void SecurityManager::initialize(const std::string& data_dir) {
    trusted_users_file_ = data_dir + "/trusted_users.json";
    load_trusted_users();
}

std::string SecurityManager::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    gmtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void SecurityManager::load_trusted_users() {
    std::ifstream file(trusted_users_file_);
    if (!file.is_open()) return;
    try {
        json j;
        file >> j;
        for (auto& entry : j) {
            TrustedUser u;
            u.user_id    = entry.value("user_id",    0LL);
            u.username   = entry.value("username",   "");
            u.first_name = entry.value("first_name", "");
            u.last_name  = entry.value("last_name",  "");
            u.phone      = entry.value("phone",      "");
            u.added_at   = entry.value("added_at",   "");
            u.blocked    = entry.value("blocked",    false);
            u.session_active = entry.value("session_active", false);
            if (u.user_id != 0) {
                trusted_users_[u.user_id] = u;
                if (u.session_active && !u.blocked) {
                    authenticated_sessions_.insert(u.user_id);
                }
            }
        }
    } catch (...) {}
}

void SecurityManager::save_trusted_users() {
    json arr = json::array();
    for (auto& [id, u] : trusted_users_) {
        arr.push_back({
            {"user_id",    u.user_id},
            {"username",   u.username},
            {"first_name", u.first_name},
            {"last_name",  u.last_name},
            {"phone",      u.phone},
            {"added_at",   u.added_at},
            {"blocked",    u.blocked},
            {"session_active", u.session_active}
        });
    }
    std::ofstream file(trusted_users_file_);
    if (file.is_open()) file << arr.dump(2);
}

std::string SecurityManager::generate_crypto_password(int length) {
    const char charset_alnum[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    const char charset_special[] = "!@#$%^&*";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_alnum(0, sizeof(charset_alnum) - 2);
    std::uniform_int_distribution<> dist_special(0, sizeof(charset_special) - 2);

    std::string pwd;
    for (int i = 0; i < length; ++i)
        pwd += charset_alnum[dist_alnum(gen)];

    // Guarantee at least 2 special chars at random positions
    std::uniform_int_distribution<> dist_pos(0, length - 1);
    int pos1 = dist_pos(gen);
    int pos2;
    do { pos2 = dist_pos(gen); } while (pos2 == pos1);
    pwd[pos1] = charset_special[dist_special(gen)];
    pwd[pos2] = charset_special[dist_special(gen)];

    return pwd;
}

// ─── Public API ──────────────────────────────────────────────────────────────

bool SecurityManager::is_trusted_user(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = trusted_users_.find(user_id);
    if (it == trusted_users_.end()) return false;
    return !it->second.blocked;   // blocked users are NOT trusted
}

bool SecurityManager::block_user(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = trusted_users_.find(user_id);
    if (it == trusted_users_.end()) return false;
    it->second.blocked = true;
    it->second.session_active = false;
    authenticated_sessions_.erase(user_id);   // kill active session
    save_trusted_users();
    Logger::instance().warning("SecurityManager: User " + std::to_string(user_id) + " BLOCKED.");
    return true;
}

bool SecurityManager::unblock_user(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = trusted_users_.find(user_id);
    if (it == trusted_users_.end()) return false;
    it->second.blocked = false;
    save_trusted_users();
    Logger::instance().info("SecurityManager: User " + std::to_string(user_id) + " un-blocked.");
    return true;
}

bool SecurityManager::is_session_authenticated(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    return authenticated_sessions_.find(user_id) != authenticated_sessions_.end();
}

bool SecurityManager::authenticate_session(long long user_id, const std::string& session_password) {
    if (session_password == g_config.get().telegram.password) {
        std::lock_guard<std::mutex> lock(mtx_);
        authenticated_sessions_.insert(user_id);
        if (trusted_users_.find(user_id) != trusted_users_.end()) {
            trusted_users_[user_id].session_active = true;
            save_trusted_users();
        }
        Logger::instance().info("SecurityManager: User " + std::to_string(user_id) + " authenticated session.");
        return true;
    }
    Logger::instance().warning("SecurityManager: Failed session auth for user " + std::to_string(user_id));
    return false;
}

bool SecurityManager::authenticate_root(long long user_id, const std::string& root_password,
                                        const std::string& username,
                                        const std::string& first_name,
                                        const std::string& last_name) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = pending_root_passwords_.find(user_id);
    if (it == pending_root_passwords_.end() || it->second != root_password) {
        Logger::instance().warning("SecurityManager: Failed root auth for user " + std::to_string(user_id));
        return false;
    }
    pending_root_passwords_.erase(it);

    // Store rich user info
    TrustedUser u;
    u.user_id    = user_id;
    u.username   = username;
    u.first_name = first_name;
    u.last_name  = last_name;
    u.added_at   = current_timestamp();
    u.blocked    = false;
    u.session_active = true;
    trusted_users_[user_id] = u;
    authenticated_sessions_.insert(user_id);
    save_trusted_users();

    Logger::instance().info("SecurityManager: User " + std::to_string(user_id)
        + " (" + first_name + " @" + username + ") is now TRUSTED.");
    return true;
}

bool SecurityManager::trigger_root_auth_flow(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (pending_root_passwords_.find(user_id) != pending_root_passwords_.end())
        return false; // already waiting

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_len(11, 15);
    std::string root_pw = generate_crypto_password(dist_len(gen));
    pending_root_passwords_[user_id] = root_pw;
    latest_root_password_            = root_pw;

    std::string alert_msg = "New connection attempt from Telegram ID: "
                          + std::to_string(user_id)
                          + "\nROOT PASSWORD: " + root_pw;

    Logger::instance().warning(alert_msg);

    // Persistent, blocking popup in its own thread
    std::thread([alert_msg]() {
        MessageBoxA(nullptr, alert_msg.c_str(),
                    "SARA Security Alert - NEW DEVICE DETECTED",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST);
    }).detach();

    return true;
}

std::string SecurityManager::get_latest_root_password() {
    std::lock_guard<std::mutex> lock(mtx_);
    return latest_root_password_;
}

std::vector<TrustedUser> SecurityManager::get_trusted_users() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<TrustedUser> result;
    for (auto& [id, u] : trusted_users_) result.push_back(u);
    return result;
}

void SecurityManager::logout(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    authenticated_sessions_.erase(user_id);
    if (trusted_users_.find(user_id) != trusted_users_.end()) {
        trusted_users_[user_id].session_active = false;
        save_trusted_users();
    }
    Logger::instance().info("SecurityManager: User " + std::to_string(user_id) + " session revoked.");
}

bool SecurityManager::check_rate_limit(long long user_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto now = std::chrono::steady_clock::now();
    auto& ctx = rate_limits_[user_id];
    if (std::chrono::duration_cast<std::chrono::seconds>(now - ctx.window_start).count() >= 1) {
        ctx.window_start = now;
        ctx.count = 0;
    }
    ctx.count++;
    if (ctx.count > 5) {
        if (ctx.count == 6)
            Logger::instance().warning("SecurityManager: Rate limit exceeded for user " + std::to_string(user_id));
        return false;
    }
    return true;
}

} // namespace sara
