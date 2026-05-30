#pragma once
#include <string>
#include <atomic>
#include <thread>

namespace sara::remote {

// TerminalHttpServer — serves the xterm.js frontend and WebSocket terminal sessions.
// Runs on port 9081 (separate from GUI WS on 9080).
//
// Routes:
//   GET /t/{session_id}?token={token}        → serve index.html (validate token)
//   GET /static/terminal.js                  → serve terminal.js
//   GET /static/terminal.css                 → serve terminal.css
//   GET /ws/{session_id}?token={token}       → WebSocket upgrade → route to PTY
//   GET /health                              → 200 OK
class TerminalHttpServer {
public:
    static TerminalHttpServer& instance() {
        static TerminalHttpServer s;
        return s;
    }

    bool start(int port = 9081, const std::string& static_dir = "");
    void stop();
    bool is_running() const { return running_; }
    int  port() const { return port_; }

private:
    TerminalHttpServer() = default;
    ~TerminalHttpServer() { stop(); }

    void accept_loop();
    void handle_client(int sock);

    // HTTP helpers
    static std::string parse_path(const std::string& request);
    static std::string parse_query_param(const std::string& request,
                                          const std::string& param);
    static bool is_websocket_upgrade(const std::string& request);
    static bool do_ws_handshake(int sock, const std::string& request);

    // Response helpers
    static void send_http(int sock, int code, const std::string& content_type,
                          const std::string& body);
    static void send_404(int sock);
    static void send_403(int sock);

    // Static file loading (loaded once at startup, kept in memory)
    static std::string load_file(const std::string& path);
    static std::string embedded_index_html();  // fallback if no file
    static std::string embedded_terminal_js();
    static std::string embedded_terminal_css();

    // WebSocket terminal loop (runs for lifetime of WS connection)
    void handle_terminal_ws(int sock,
                             const std::string& session_id,
                             const std::string& token);

    int port_ = 9081;
    std::string static_dir_;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    // Cached static files
    std::string html_content_;
    std::string js_content_;
    std::string css_content_;
};

} // namespace sara::remote
