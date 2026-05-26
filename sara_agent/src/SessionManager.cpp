#include "../include/SessionManager.h"
#include "../include/Logger.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>

static std::string gen_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<unsigned long long> dis;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dis(gen)
        << std::setw(16) << std::setfill('0') << dis(gen);
    return oss.str();
}

namespace sara {

std::string SessionManager::generate_session_id() {
    return gen_uuid();
}

Session& SessionManager::get_or_create(const std::string& platform,
    const std::string& chat_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = platform + ":" + chat_id;
    for (auto& [id, session] : sessions_) {
        if (session.platform == platform && session.chat_id == chat_id) {
            session.last_activity = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return session;
        }
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    Session s;
    s.session_id = generate_session_id();
    s.platform = platform;
    s.chat_id = chat_id;
    s.created_at = now;
    s.last_activity = now;
    sessions_[s.session_id] = s;

    return sessions_[s.session_id];
}

Session* SessionManager::get(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) return &it->second;
    return nullptr;
}

void SessionManager::update_activity(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second.last_activity = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void SessionManager::add_command(const std::string& session_id, const json& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    it->second.recent_commands.push_back(command);
    if (it->second.recent_commands.size() > 20) {
        it->second.recent_commands.erase(it->second.recent_commands.begin());
    }
}

void SessionManager::add_history(const std::string& session_id, const std::string& role, const std::string& content) {
    if (content.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    
    it->second.history.push_back({role, content});
    // Keep last 30 messages to support long agentic chains
    if (it->second.history.size() > 30) {
        it->second.history.erase(it->second.history.begin());
    }
}

void SessionManager::clear_history(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    it->second.history.clear();
    it->second.recent_commands.clear();
}

void SessionManager::set_last_target(const std::string& session_id,
    const std::string& target)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    auto& targets = it->second.last_targets;
    targets.erase(std::remove(targets.begin(), targets.end(), target), targets.end());
    targets.push_back(target);
    if (targets.size() > 10) targets.erase(targets.begin());
}

std::string SessionManager::get_last_target(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end() || it->second.last_targets.empty()) return "";
    return it->second.last_targets.back();
}

json SessionManager::get_context(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return json::object();

    json ctx;
    ctx["session_id"] = it->second.session_id;
    ctx["platform"] = it->second.platform;
    ctx["last_targets"] = it->second.last_targets;
    ctx["recent_commands"] = it->second.recent_commands;
    
    json hist = json::array();
    for (auto& m : it->second.history) {
        hist.push_back({{"role", m.role}, {"content", m.content}});
    }
    ctx["history"] = hist;
    
    return ctx;
}

void SessionManager::cleanup_expired(int max_idle_minutes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto cutoff = now - (max_idle_minutes * 60);

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.last_activity < cutoff) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

}
