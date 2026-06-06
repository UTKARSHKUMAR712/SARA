#pragma once
#include "ConPTYSession.h"
#include "TerminalToken.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

namespace sara::remote {

// ── Session State ─────────────────────────────────────────────────────────────
struct TerminalSession {
    std::string id;            // 8-char alphanumeric
    std::string token;         // 64-char hex, cryptographically random
    std::string owner_chat_id; // Telegram chat ID that created this session
    bool        admin_mode;    // Was launched as admin shell
    std::string shell_cmd;     // e.g. "powershell.exe" or "cmd.exe"

    ConPTYSession pty;

    // WebSocket socket (raw fd). -1 = no browser attached.
    // Protected by TerminalSessionManager::sessions_mutex_
    int ws_socket = -1;

    // Buffered PTY output accumulated before WebSocket attaches.
    // Flushed to the browser when attach_websocket() is called.
    std::string pending_output;

    int64_t created_at  = 0;
    int64_t last_active = 0;   // updated on every PTY I/O
    int64_t expires_at  = 0;   // created_at + expiry_minutes*60

    bool expired() const {
        return now_seconds() > expires_at;
    }

    // Non-copyable because ConPTYSession is non-copyable
    TerminalSession() = default;
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
    TerminalSession(TerminalSession&&) = default;
    TerminalSession& operator=(TerminalSession&&) = default;
};

// ── Result from create_session ────────────────────────────────────────────────
struct CreateSessionResult {
    std::string session_id;
    std::string token;
    bool        success = false;
    std::string error;
};

// ── TerminalSessionManager — singleton ────────────────────────────────────────
class TerminalSessionManager {
public:
    static TerminalSessionManager& instance() {
        static TerminalSessionManager s;
        return s;
    }

    // Creates a new PTY session. Returns {session_id, token} on success.
    CreateSessionResult create_session(const std::string& chat_id,
                                       const std::string& shell_cmd,
                                       int cols, int rows,
                                       bool admin_mode,
                                       int expiry_minutes = 30);

    // Validate token and attach a WebSocket socket to the session's PTY.
    // The TerminalHttpServer calls this after WS upgrade.
    // Returns true on success.
    bool attach_websocket(const std::string& session_id,
                          const std::string& token,
                          int ws_sock);

    // Detach WebSocket (browser closed). PTY keeps running.
    void detach_websocket(const std::string& session_id);

    // Send raw bytes to the PTY stdin (browser → PTY).
    void pty_write(const std::string& session_id, const std::string& data);

    // Resize PTY (browser resize event).
    void pty_resize(const std::string& session_id, int cols, int rows);

    // Destroy a session explicitly (e.g. /killterminal).
    void destroy_session(const std::string& session_id,
                         const std::string& chat_id = ""); // chat_id="" = force

    // Called by TerminalHttpServer to validate token before serving page.
    bool validate_token(const std::string& session_id,
                        const std::string& token) const;

    // Validate that a token belongs to ANY active session (for /api/new_session and /files proxy).
    bool validate_any_token(const std::string& token) const;

    // Issue a short-lived auth token WITHOUT spawning a PTY.
    // Used for /files File Browser authentication.
    // Returns {session_id, token, true} on success.
    CreateSessionResult issue_auth_token(const std::string& chat_id,
                                         int expiry_minutes = 120,
                                         const std::string& token_type = "files");

    // Cleanup expired sessions (called every 60s by background thread).
    void cleanup_expired();

    // Destroy all sessions that match a specific token_type.
    void destroy_sessions_by_type(const std::string& type);

    // Shutdown all sessions.
    void shutdown_all();

    // Get active session count.
    int session_count() const;

    // Start background cleanup thread.
    void start_cleanup_thread(int interval_seconds = 60);
    void stop_cleanup_thread();

private:
    TerminalSessionManager() = default;
    ~TerminalSessionManager() { shutdown_all(); stop_cleanup_thread(); }

    // Send a WS frame to attached browser. Call with sessions_mutex_ held.
    static void ws_send_raw(int sock, const std::string& json_text);

    mutable std::mutex sessions_mutex_;
    std::unordered_map<std::string, std::unique_ptr<TerminalSession>> sessions_;

    std::atomic<bool> cleanup_running_{false};
    std::thread cleanup_thread_;
};

} // namespace sara::remote
