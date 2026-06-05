#include "../include/TerminalSessionManager.h"
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <memory>

// ── WS frame helper ────────────────────────────────────────────────────────────
static std::string b64_encode(const unsigned char* data, size_t len) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = (unsigned int)data[i] << 16;
        if (i+1 < len) v |= (unsigned int)data[i+1] << 8;
        if (i+2 < len) v |= data[i+2];
        out += t[(v>>18)&0x3F]; out += t[(v>>12)&0x3F];
        out += (i+1<len)?t[(v>>6)&0x3F]:'='; out += (i+2<len)?t[v&0x3F]:'=';
    }
    return out;
}

static std::string ws_frame_text(const std::string& payload) {
    std::vector<char> frame;
    frame.push_back('\x81');
    size_t l = payload.size();
    if (l < 126) frame.push_back((char)l);
    else if (l < 65536) { frame.push_back('\x7E'); frame.push_back((char)(l>>8)); frame.push_back((char)(l&0xFF)); }
    else { frame.push_back('\x7F'); for(int i=7;i>=0;i--) frame.push_back((char)((l>>(i*8))&0xFF)); }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return std::string(frame.data(), frame.size());
}

namespace sara::remote {

void TerminalSessionManager::ws_send_raw(int sock, const std::string& json_text) {
    if (sock < 0) return;
    std::string frame = ws_frame_text(json_text);
    ::send(sock, frame.data(), (int)frame.size(), 0);
}

CreateSessionResult TerminalSessionManager::create_session(
    const std::string& chat_id,
    const std::string& shell_cmd,
    int cols, int rows,
    bool admin_mode,
    int expiry_minutes)
{
    auto sid   = generate_session_id();
    auto token = generate_token();
    int64_t now = now_seconds();

    auto sess = std::make_unique<TerminalSession>();
    sess->id             = sid;
    sess->token          = token;
    sess->owner_chat_id  = chat_id;
    sess->admin_mode     = admin_mode;
    sess->shell_cmd      = shell_cmd;
    sess->created_at     = now;
    sess->last_active    = now;
    sess->expires_at     = now + (int64_t)expiry_minutes * 60;
    sess->ws_socket      = -1;

    auto* mgr     = this;
    auto* raw_ptr = sess.get();  // safe: ptr lives as long as sessions_ entry

    bool started = raw_ptr->pty.start(shell_cmd, cols, rows, admin_mode,
        // Output callback: encode raw PTY bytes as base64 JSON, send via WS
        [mgr, sid](const std::string& raw_data) {
            std::string b64 = b64_encode(
                reinterpret_cast<const unsigned char*>(raw_data.data()),
                raw_data.size());
            std::string json = "{\"type\":\"output\",\"data\":\"" + b64 + "\"}";
            std::lock_guard<std::mutex> lock(mgr->sessions_mutex_);
            auto it = mgr->sessions_.find(sid);
            if (it != mgr->sessions_.end()) {
                it->second->last_active = now_seconds();
                if (it->second->ws_socket >= 0) {
                    ws_send_raw(it->second->ws_socket, json);
                } else {
                    // No browser attached yet — buffer the output so it's not lost
                    it->second->pending_output += json;
                    // Cap buffer at 1MB to prevent memory bloat
                    if (it->second->pending_output.size() > 1024 * 1024)
                        it->second->pending_output.erase(0, it->second->pending_output.size() - 512 * 1024);
                }
            }
        },
        // Exit callback
        [mgr, sid](int exit_code) {
            std::string json = "{\"type\":\"exit\",\"code\":" + std::to_string(exit_code) + "}";
            std::lock_guard<std::mutex> lock(mgr->sessions_mutex_);
            auto it = mgr->sessions_.find(sid);
            if (it != mgr->sessions_.end()) {
                ws_send_raw(it->second->ws_socket, json);
            }
        }
    );

    if (!started) {
        return {sid, token, false, "Failed to create ConPTY session"};
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[sid] = std::move(sess);
    }

    return {sid, token, true, ""};
}

bool TerminalSessionManager::attach_websocket(
    const std::string& session_id,
    const std::string& token,
    int ws_sock)
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return false;
    if (it->second->token != token) return false;
    if (it->second->expired()) return false;
    if (!it->second->pty.is_alive()) return false;

    it->second->ws_socket = ws_sock;
    it->second->last_active = now_seconds();

    // Flush any PTY output that accumulated before the browser connected
    if (!it->second->pending_output.empty()) {
        std::string buffered = std::move(it->second->pending_output);
        it->second->pending_output.clear();
        ws_send_raw(ws_sock, buffered);
    }

    return true;
}

void TerminalSessionManager::detach_websocket(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) it->second->ws_socket = -1;
}

void TerminalSessionManager::pty_write(const std::string& session_id,
                                        const std::string& data) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    it->second->last_active = now_seconds();
    it->second->pty.write(data);
}

void TerminalSessionManager::pty_resize(const std::string& session_id,
                                         int cols, int rows) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    it->second->pty.resize(cols, rows);
}

bool TerminalSessionManager::validate_token(const std::string& session_id,
                                              const std::string& token) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return false;
    if (it->second->token != token) return false;
    if (it->second->expired()) return false;
    return true;
}

bool TerminalSessionManager::validate_any_token(const std::string& token) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& [id, sess] : sessions_) {
        if (sess->token == token && !sess->expired()) return true;
    }
    return false;
}

CreateSessionResult TerminalSessionManager::issue_auth_token(
    const std::string& chat_id, int expiry_minutes)
{
    auto sid   = generate_session_id();
    auto token = generate_token();
    int64_t now = now_seconds();

    auto sess = std::make_unique<TerminalSession>();
    sess->id             = sid;
    sess->token          = token;
    sess->owner_chat_id  = chat_id;
    sess->admin_mode     = false;
    sess->shell_cmd      = "files"; // marker — no PTY
    sess->created_at     = now;
    sess->last_active    = now;
    sess->expires_at     = now + (int64_t)expiry_minutes * 60;
    sess->ws_socket      = -1;
    // NOTE: pty is NOT started — this session exists only for token validation.

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[sid] = std::move(sess);
    }
    return {sid, token, true, ""};
}

void TerminalSessionManager::destroy_session(const std::string& session_id,
                                              const std::string& chat_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;
    if (!chat_id.empty() && it->second->owner_chat_id != chat_id) return;

    ws_send_raw(it->second->ws_socket,
        "{\"type\":\"exit\",\"code\":-1,\"reason\":\"Session destroyed\"}");
    it->second->pty.stop();
    sessions_.erase(it);
}

void TerminalSessionManager::cleanup_expired() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (it->second->expired()) {
            ws_send_raw(it->second->ws_socket,
                "{\"type\":\"exit\",\"code\":-2,\"reason\":\"Session expired\"}");
            it->second->pty.stop();
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void TerminalSessionManager::shutdown_all() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, sess] : sessions_) {
        ws_send_raw(sess->ws_socket,
            "{\"type\":\"exit\",\"code\":-3,\"reason\":\"SARA shutting down\"}");
        sess->pty.stop();
    }
    sessions_.clear();
}

int TerminalSessionManager::session_count() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return (int)sessions_.size();
}

void TerminalSessionManager::start_cleanup_thread(int interval_seconds) {
    cleanup_running_ = true;
    cleanup_thread_ = std::thread([this, interval_seconds]() {
        while (cleanup_running_) {
            for (int i = 0; i < interval_seconds && cleanup_running_; i++)
                Sleep(1000);
            if (cleanup_running_) cleanup_expired();
        }
    });
}

void TerminalSessionManager::stop_cleanup_thread() {
    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) cleanup_thread_.join();
}

} // namespace sara::remote
