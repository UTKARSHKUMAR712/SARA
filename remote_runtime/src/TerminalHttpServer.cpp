#include "../include/TerminalHttpServer.h"
#include "../include/TerminalSessionManager.h"
#include "../include/FileBrowserManager.h"
#include "../include/ApiFsController.h"
#include "../include/ApiGitController.h"
#include "../include/ApiSearchController.h"
#include "../include/ApiWorkspaceController.h"
#include "../include/HttpUtils.h"
#include <filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

using namespace sara::remote::http;
using namespace sara::remote::api;

// ── WS frame helpers ──────────────────────────────────────────────────────────
static std::string ws_make_frame(const std::string& payload) {
    std::vector<char> f;
    f.push_back('\x81');
    size_t l = payload.size();
    if (l < 126) f.push_back((char)l);
    else if (l < 65536) { f.push_back('\x7E'); f.push_back((char)(l>>8)); f.push_back((char)(l&0xFF)); }
    else { f.push_back('\x7F'); for(int i=7;i>=0;i--) f.push_back((char)((l>>(i*8))&0xFF)); }
    f.insert(f.end(), payload.begin(), payload.end());
    return std::string(f.data(), f.size());
}

static bool recv_exact(int sock, char* buf, int len) {
    int rx = 0;
    while (rx < len) {
        int n = recv(sock, buf + rx, len - rx, 0);
        if (n <= 0) return false;
        rx += n;
    }
    return true;
}

static std::string ws_read_frame(int sock) {
    char hdr[2]; if (!recv_exact(sock, hdr, 2)) return "";
    bool masked = hdr[1] & 0x80;
    uint64_t len = hdr[1] & 0x7F;
    if (len == 126) { char e[2]; if(!recv_exact(sock,e,2))return""; len=((unsigned char)e[0]<<8)|(unsigned char)e[1]; }
    else if (len == 127) { char e[8]; if(!recv_exact(sock,e,8))return""; len=0; for(int i=0;i<8;i++) len=(len<<8)|(unsigned char)e[i]; }
    char mask[4]={0}; if (masked && !recv_exact(sock, mask, 4)) return "";
    if (len > 1024*1024) return ""; // 1MB max
    std::vector<char> payload(len);
    if (len > 0 && !recv_exact(sock, payload.data(), (int)len)) return "";
    if (masked) for (uint64_t i=0;i<len;i++) payload[i]^=mask[i%4];
    if ((unsigned char)hdr[0]==0x88) return ""; // close frame
    return std::string(payload.data(), len);
}

namespace sara::remote {

bool TerminalHttpServer::start(int port, const std::string& static_dir) {
    if (running_) return true;
    port_       = port;
    static_dir_ = static_dir;

    // Load static files into memory (or use embedded fallbacks)
    auto try_load = [&](const std::string& name) -> std::string {
        if (!static_dir.empty()) {
            std::string path = static_dir + "\\" + name;
            std::ifstream f(path, std::ios::binary);
            if (f) return {std::istreambuf_iterator<char>(f), {}};
        }
        return "";
    };
    html_content_ = try_load("index.html");
    js_content_   = try_load("terminal.js");
    css_content_  = try_load("terminal.css");
    if (html_content_.empty()) html_content_ = embedded_index_html();
    if (js_content_.empty())   js_content_   = embedded_terminal_js();
    if (css_content_.empty())  css_content_  = embedded_terminal_css();

    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(server_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(server_sock); return false;
    }
    listen(server_sock, 16);
    running_ = true;

    accept_thread_ = std::thread([this, server_sock]() {
        while (running_) {
            fd_set fds; FD_ZERO(&fds); FD_SET(server_sock, &fds);
            timeval tv{0, 500000};
            if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
            sockaddr_in ca{}; int cl = sizeof(ca);
            SOCKET cs = accept(server_sock, (sockaddr*)&ca, &cl);
            if (cs == INVALID_SOCKET) continue;
            std::thread([this, cs]() { handle_client(cs); }).detach();
        }
        closesocket(server_sock);
    });
    accept_thread_.detach();
    return true;
}

void TerminalHttpServer::stop() {
    running_ = false;
}

void TerminalHttpServer::handle_client(SOCKET sock) {
    // Read HTTP request
    char buf[8192]; std::string req;
    
    // Cloudflared opens connection pools and keeps them idle. 
    // We must wait for data instead of killing the socket after 1 second.
    while (true) {
        fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);
        timeval tv{60, 0}; // 60 second timeout
        int s = select(0, &fds, nullptr, nullptr, &tv);
        if (s <= 0) break; // timeout or error

        int n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n > 0) { 
            buf[n]=0; req += buf; 
            if (req.find("\r\n\r\n") != std::string::npos) break; 
        }
        else { break; } // connection closed by client
    }
    
    if (req.empty() || req.find("\r\n\r\n") == std::string::npos) { 
        closesocket(sock); return; 
    }

    // DEBUG LOGGING
    {
        std::ofstream dbg("http_debug.log", std::ios::app);
        dbg << "=== NEW REQUEST ===\n" << req << "\n===================\n";
    }

    std::string path  = parse_path(req);
    std::string token = parse_query_param(req, "token");

    // ── WebSocket upgrade for terminal ────────────────────────────────────────
    if (is_websocket_upgrade(req) && path.rfind("/ws/", 0) == 0) {
        std::string sid = path.substr(4);
        // Strip any trailing query
        auto q = sid.find('?'); if (q != std::string::npos) sid = sid.substr(0, q);
        if (!do_ws_handshake(sock, req)) { closesocket(sock); return; }
        handle_terminal_ws(sock, sid, token);
        return;
    }

    // ── New session API (for tabs/splits from browser) ─────────────────────────
    if (path == "/api/new_session") {
        std::string parent_token = parse_query_param(req, "parent_token");
        std::string shell = parse_query_param(req, "shell");
        if (parent_token.empty() || shell.empty()) {
            std::string err = "{\"error\":\"Missing parent_token or shell\"}";
            send_http(sock, 400, "application/json; charset=utf-8", err);
            closesocket(sock); return;
        }
        if (!TerminalSessionManager::instance().validate_any_token(parent_token)) {
            std::string err = "{\"error\":\"Invalid or expired token\"}";
            send_http(sock, 403, "application/json; charset=utf-8", err);
            closesocket(sock); return;
        }
        auto result = TerminalSessionManager::instance().create_session(
            "", shell, 120, 40, false, 30);
        if (result.success) {
            std::string json = "{\"session_id\":\"" + result.session_id +
                               "\",\"token\":\"" + result.token + "\"}";
            send_http(sock, 200, "application/json; charset=utf-8", json);
        } else {
            std::string err = "{\"error\":\"" + (result.error.empty() ? "Failed to create session" : result.error) + "\"}";
            send_http(sock, 500, "application/json; charset=utf-8", err);
        }
        closesocket(sock); return;
    }

    // ── Static files ──────────────────────────────────────────────────────────
    if (path == "/static/terminal.js") {
        send_http(sock, 200, "application/javascript; charset=utf-8", js_content_); closesocket(sock); return;
    }
    if (path == "/static/terminal.css") {
        send_http(sock, 200, "text/css; charset=utf-8", css_content_); closesocket(sock); return;
    }

    // ── Terminal page ─────────────────────────────────────────────────────────
    if (path.rfind("/t/", 0) == 0) {
        std::string sid = path.substr(3);
        auto q = sid.find('?'); if (q != std::string::npos) sid = sid.substr(0, q);
        if (!TerminalSessionManager::instance().validate_token(sid, token)) {
            send_403(sock); closesocket(sock); return;
        }
        send_http(sock, 200, "text/html; charset=utf-8", html_content_); closesocket(sock); return;
    }

    // Token validation helper
    auto validate_request_token = [&]() -> bool {
        std::string cookie_token;
        std::string lower_req = req;
        std::transform(lower_req.begin(), lower_req.end(), lower_req.begin(), ::tolower);
        {
            auto cp = lower_req.find("sara-token:");
            if (cp != std::string::npos) {
                cp += 11;
                while (cp < req.size() && req[cp] == ' ') cp++;
                auto ep = req.find("\r\n", cp);
                cookie_token = ep != std::string::npos ? req.substr(cp, ep - cp) : req.substr(cp);
            }
        }
        if (cookie_token.empty()) {
            auto cp = lower_req.find("cookie:");
            if (cp != std::string::npos) {
                cp += 7;
                auto ep = req.find("\r\n", cp);
                std::string cookie_hdr = ep != std::string::npos ? req.substr(cp, ep - cp) : req.substr(cp);
                auto tp = cookie_hdr.find("sara_token=");
                if (tp != std::string::npos) {
                    tp += 11;
                    auto tend = cookie_hdr.find(';', tp);
                    cookie_token = tend != std::string::npos ? cookie_hdr.substr(tp, tend - tp) : cookie_hdr.substr(tp);
                    while (!cookie_token.empty() && (cookie_token.front() == ' ')) cookie_token.erase(0,1);
                    while (!cookie_token.empty() && (cookie_token.back() == ' ' || cookie_token.back() == '\r')) cookie_token.pop_back();
                }
            }
        }
        std::string auth_token = token.empty() ? cookie_token : token;
        return TerminalSessionManager::instance().validate_any_token(auth_token);
    };

    // ── Workspace Static Files & API ──────────────────────────────────────────
    if (path.rfind("/workspace", 0) == 0) {
        if (!validate_request_token()) {
            send_403(sock); closesocket(sock); return;
        }

        std::string root_dir = FileBrowserManager::instance().root_dir();
        if (root_dir.empty()) root_dir = ".";

        if (path == "/workspace/api/search_code") {
            handle_search_api_request(sock, req, path);
            closesocket(sock); return;
        } else if (path.find("/workspace/api/live_server/") == 0 ||
                   path.find("/workspace/api/ports/") == 0 ||
                   path.find("/workspace/api/settings") == 0 ||
                   path == "/workspace/api/ports") {
            std::string action;
            if (path.find("/workspace/api/live_server/") == 0) action = "live_server";
            else if (path.find("/workspace/api/ports") == 0) action = "ports";
            else action = "settings";
            
            handle_workspace_api_request(sock, req, path, action);
            closesocket(sock); return;
        } else if (path.find("/workspace/api/git/") == 0) {
            std::string action = path.substr(15); // "git/..."
            auto qm = action.find('?');
            if (qm != std::string::npos) action = action.substr(0, qm);
            std::string git_action = action.substr(4); // past "git/"

            std::string workspace_path = root_dir;
            std::string query_ws = url_decode(parse_query_param(req, "workspace"));
            if (!query_ws.empty()) {
                workspace_path = query_ws;
            }
            workspace_path = resolve_workspace_path(root_dir, workspace_path);

            handle_git_api_request(sock, req, git_action, workspace_path);
            closesocket(sock); return;
        } else if (path.find("/workspace/api/") == 0) {
            std::string action = path.substr(15);
            auto qm = action.find('?');
            if (qm != std::string::npos) action = action.substr(0, qm);

            handle_fs_api_request(sock, req, action, root_dir);
            closesocket(sock); return;
        }

        std::string req_file = path.substr(10); // remove "/workspace"
        if (req_file.empty() || req_file == "/") req_file = "/index.html";
        
        // Prevent path traversal
        if (req_file.find("..") != std::string::npos) {
            send_403(sock); closesocket(sock); return;
        }

        std::string workspace_dir = static_dir_.empty() ? "remote_runtime\\workspace" : static_dir_ + "\\..\\workspace";
        std::string full_path = workspace_dir + req_file;
        for (char& c : full_path) if (c == '/') c = '\\';

        std::ifstream f(full_path, std::ios::binary);
        if (!f) {
            send_404(sock); closesocket(sock); return;
        }
        std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        std::string ct = "text/plain";
        if (full_path.ends_with(".html")) ct = "text/html; charset=utf-8";
        else if (full_path.ends_with(".js")) ct = "application/javascript; charset=utf-8";
        else if (full_path.ends_with(".css")) ct = "text/css; charset=utf-8";
        else if (full_path.ends_with(".json")) ct = "application/json; charset=utf-8";
        else if (full_path.ends_with(".png")) ct = "image/png";
        else if (full_path.ends_with(".svg")) ct = "image/svg+xml";
        else if (full_path.ends_with(".ttf")) ct = "font/ttf";
        else if (full_path.ends_with(".woff")) ct = "font/woff";
        else if (full_path.ends_with(".woff2")) ct = "font/woff2";

        std::string extra_headers = "";
        if (!token.empty()) {
            extra_headers = "Set-Cookie: sara_token=" + token + "; Path=/; SameSite=Lax\r\n";
        }

        send_http(sock, 200, ct, body, extra_headers);
        closesocket(sock);
        return;
    }

    // ── Source Control Standalone App ─────────────────────────────────────────
    if (path.rfind("/sourcecontrol", 0) == 0) {
        if (!validate_request_token()) {
            send_403(sock); closesocket(sock); return;
        }

        std::string req_file = path.substr(14); // remove "/sourcecontrol"
        if (req_file.empty() || req_file == "/") req_file = "/index.html";
        
        // Prevent path traversal
        if (req_file.find("..") != std::string::npos) {
            send_403(sock); closesocket(sock); return;
        }

        std::string sc_dir = static_dir_.empty() ? "remote_runtime\\sourcecontrol" : static_dir_ + "\\..\\sourcecontrol";
        std::string full_path = sc_dir + req_file;
        for (char& c : full_path) if (c == '/') c = '\\';

        std::ifstream f(full_path, std::ios::binary);
        if (!f) {
            send_404(sock); closesocket(sock); return;
        }
        std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        std::string ct = "text/plain";
        if (full_path.ends_with(".html")) ct = "text/html; charset=utf-8";
        else if (full_path.ends_with(".js")) ct = "application/javascript; charset=utf-8";
        else if (full_path.ends_with(".css")) ct = "text/css; charset=utf-8";
        else if (full_path.ends_with(".json")) ct = "application/json; charset=utf-8";

        std::string extra_headers = "";
        if (!token.empty()) {
            extra_headers = "Set-Cookie: sara_token=" + token + "; Path=/; SameSite=Lax\r\n";
        }

        send_http(sock, 200, ct, body, extra_headers);
        closesocket(sock);
        return;
    }

    // ── File Browser reverse proxy ───────────────────────────────────────────
    if (path.rfind("/files", 0) == 0) {
        if (!validate_request_token()) {
            send_403(sock); closesocket(sock); return;
        }
        proxy_to_filebrowser(sock, req, path);
        return;
    }

    // ── Reverse Proxy /preview/{port}/ ───────────────────────────────────────
    if (path.find("/preview/") == 0) {
        if (!validate_request_token()) { send_403(sock); closesocket(sock); return; }
        auto slash_idx = path.find('/', 9);
        std::string port_str = path.substr(9, slash_idx == std::string::npos ? std::string::npos : slash_idx - 9);
        int port = 0;
        try { port = std::stoi(port_str); } catch(...) {}
        if (port > 0) {
            std::string prefix = "/preview/" + port_str;
            proxy_to_port(sock, req, port, prefix);
            return;
        }
    }

    // Block Service Workers from being registered on the domain
    if (path.ends_with("sw.js") || path.ends_with("service-worker.js")) {
        send_404(sock); closesocket(sock); return;
    }

    // ── Referer-based fallback for Preview ───────────────────────────────────
    {
        std::string lower_req = req;
        std::transform(lower_req.begin(), lower_req.end(), lower_req.begin(), ::tolower);
        auto ref_pos = lower_req.find("referer:");
        if (ref_pos != std::string::npos) {
            auto eol = lower_req.find("\r\n", ref_pos);
            std::string referer = req.substr(ref_pos + 8, eol - (ref_pos + 8));
            auto prev_pos = referer.find("/preview/");
            if (prev_pos != std::string::npos) {
                auto slash_idx = referer.find('/', prev_pos + 9);
                std::string port_str = referer.substr(prev_pos + 9, slash_idx == std::string::npos ? std::string::npos : slash_idx - (prev_pos + 9));
                int port = 0;
                try { port = std::stoi(port_str); } catch(...) {}
                if (port > 0 && path.find("/workspace") != 0 && path.find("/api") != 0 && path.find("/t/") != 0 && path.find("/static/") != 0 && path != "/health") {
                    proxy_to_port(sock, req, port, ""); // no prefix to strip since path is absolute (e.g., /main.js)
                    return;
                }
            }
        }
    }

    // ── Health check ──────────────────────────────────────────────────────────
    if (path == "/health") {
        send_http(sock, 200, "text/plain", "ok"); closesocket(sock); return;
    }

    send_404(sock); closesocket(sock);
}

void TerminalHttpServer::handle_terminal_ws(SOCKET sock,
                                             const std::string& session_id,
                                             const std::string& token) {
    // Validate and attach WebSocket to session
    if (!TerminalSessionManager::instance().attach_websocket(session_id, token, sock)) {
        // Send error frame and close
        std::string err = ws_make_frame("{\"type\":\"error\",\"reason\":\"Invalid session or token\"}");
        ::send(sock, err.data(), (int)err.size(), 0);
        closesocket(sock);
        return;
    }

    // Loop: read browser input → forward to PTY
    // PTY output is pushed to this sock by TerminalSessionManager's output callback
    while (running_) {
        fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds);
        timeval tv{2, 0};
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) {
            // Check if session still alive
            if (!TerminalSessionManager::instance().validate_token(session_id, token)) break;
            continue;
        }

        std::string frame = ws_read_frame(sock);
        if (frame.empty()) break; // client disconnected

        // Parse JSON message from browser
        // { "type": "input",  "data": "<base64>" }
        // { "type": "resize", "cols": N, "rows": N }
        // { "type": "ping" }
        try {
            // Simple manual JSON parse (avoid nlohmann dependency in remote_runtime)
            auto get_str = [&](const std::string& key) -> std::string {
                auto p = frame.find("\"" + key + "\":");
                if (p == std::string::npos) return "";
                p += key.size() + 3;
                while (p < frame.size() && frame[p] == ' ') p++;
                if (p >= frame.size()) return "";
                if (frame[p] == '"') {
                    p++; auto e = frame.find('"', p);
                    return e != std::string::npos ? frame.substr(p, e-p) : "";
                }
                auto e = frame.find_first_of(",}", p);
                return e != std::string::npos ? frame.substr(p, e-p) : frame.substr(p);
            };
            auto get_int = [&](const std::string& key) -> int {
                std::string s = get_str(key);
                try { return std::stoi(s); } catch(...) { return 0; }
            };

            std::string type = get_str("type");

            if (type == "input") {
                std::string b64 = get_str("data");
                // Decode base64
                static const std::string b64chars =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string decoded;
                decoded.reserve(b64.size() * 3 / 4);
                int val = 0, bits = -8;
                for (unsigned char c : b64) {
                    auto p2 = b64chars.find(c);
                    if (p2 == std::string::npos) continue;
                    val = (val << 6) + (int)p2; bits += 6;
                    if (bits >= 0) { decoded += (char)((val >> bits) & 0xFF); bits -= 8; }
                }
                TerminalSessionManager::instance().pty_write(session_id, decoded);
            } else if (type == "resize") {
                int cols = get_int("cols");
                int rows = get_int("rows");
                if (cols > 0 && rows > 0)
                    TerminalSessionManager::instance().pty_resize(session_id, cols, rows);
            } else if (type == "ping") {
                std::string pong = ws_make_frame("{\"type\":\"pong\"}");
                ::send(sock, pong.data(), (int)pong.size(), 0);
            }
        } catch (...) {}
    }

    TerminalSessionManager::instance().detach_websocket(session_id);
    closesocket(sock);
}


// ───────────────────────────────────────────────────────────────────────────────
// FILE BROWSER REVERSE PROXY
// ───────────────────────────────────────────────────────────────────────────────
void TerminalHttpServer::proxy_to_filebrowser(SOCKET client_sock,
                                               const std::string& raw_request,
                                               const std::string& path) {
    SOCKET fb_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (fb_sock == INVALID_SOCKET) {
        std::string body502 = "502 Bad Gateway — File Browser not reachable";
        std::string resp502 =
            "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain\r\nContent-Length: " +
            std::to_string(body502.size()) + "\r\nConnection: close\r\n\r\n" + body502;
        ::send(client_sock, resp502.data(), (int)resp502.size(), 0);
        closesocket(client_sock); return;
    }

    sockaddr_in fb_addr{};
    fb_addr.sin_family      = AF_INET;
    fb_addr.sin_addr.s_addr = htonl(0x7F000001); // 127.0.0.1
    fb_addr.sin_port        = htons((u_short)9090);

    if (connect(fb_sock, (sockaddr*)&fb_addr, sizeof(fb_addr)) == SOCKET_ERROR) {
        closesocket(fb_sock);
        send_http(client_sock, 503, "text/plain",
            "503 File Browser unavailable. It may still be starting up — please retry in a moment.");
        closesocket(client_sock);
        return;
    }

    std::string fwd_req = raw_request;
    {
        auto line_end = fwd_req.find("\r\n");
        if (line_end != std::string::npos) {
            std::string req_line = fwd_req.substr(0, line_end);
            auto tp = req_line.find("?token=");
            if (tp != std::string::npos) {
                auto space_after = req_line.find(' ', tp);
                if (space_after != std::string::npos)
                    req_line = req_line.substr(0, tp) + req_line.substr(space_after);
                else
                    req_line = req_line.substr(0, tp) + " HTTP/1.1";
            }
            auto ap = req_line.find("&token=");
            if (ap != std::string::npos) {
                auto amp_or_space = req_line.find_first_of(" &", ap + 1);
                if (amp_or_space != std::string::npos)
                    req_line = req_line.substr(0, ap) + req_line.substr(amp_or_space);
                else
                    req_line = req_line.substr(0, ap);
            }
            fwd_req = req_line + fwd_req.substr(line_end);
        }
    }
    {
        auto strip_header = [&](const std::string& prefix) {
            size_t pos = 0;
            while ((pos = fwd_req.find(prefix, pos)) != std::string::npos) {
                if (pos == 0 || fwd_req[pos-1] == '\n') {
                    size_t end = fwd_req.find("\r\n", pos);
                    if (end != std::string::npos) fwd_req.erase(pos, end - pos + 2);
                    else break;
                } else pos += prefix.size();
            }
        };
        strip_header("X-Forwarded-For:");
        strip_header("x-forwarded-for:");
        strip_header("X-Real-IP:");
        strip_header("x-real-ip:");
        strip_header("X-Forwarded-Proto:");
        strip_header("x-forwarded-proto:");

        
        auto cp = fwd_req.find("Connection:");
        if (cp == std::string::npos) cp = fwd_req.find("connection:");
        if (cp != std::string::npos) {
            auto ep = fwd_req.find("\r\n", cp);
            if (ep != std::string::npos) fwd_req.erase(cp, ep - cp + 2);
        }
        auto hdr_end = fwd_req.find("\r\n\r\n");
        if (hdr_end != std::string::npos) {
            std::string inject = "X-Forwarded-For: 127.0.0.1\r\nX-Real-IP: 127.0.0.1\r\nX-Remote-User: admin\r\nConnection: close\r\n";
            fwd_req.insert(hdr_end + 2, inject);
        }
    }

    int sent = 0;
    while (sent < (int)fwd_req.size()) {
        int n = ::send(fb_sock, fwd_req.data() + sent, (int)(fwd_req.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }

    auto pipe_thread = [](SOCKET from, SOCKET to, std::atomic<bool>& done_flag) {
        char buf[8192];
        while (!done_flag) {
            fd_set fds; FD_ZERO(&fds); FD_SET(from, &fds);
            timeval tv{1, 0};
            if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
            int n = recv(from, buf, sizeof(buf), 0);
            if (n <= 0) { done_flag = true; break; }
            int s = 0;
            while (s < n) {
                int r = ::send(to, buf + s, n - s, 0);
                if (r <= 0) { done_flag = true; return; }
                s += r;
            }
        }
    };

    std::string echo_token = parse_query_param(raw_request, "token");
    bool is_ws = is_websocket_upgrade(raw_request);
    
    if (is_ws || echo_token.empty()) {
        std::atomic<bool> done{false};
        std::thread t1(pipe_thread, fb_sock, client_sock, std::ref(done));
        std::thread t2(pipe_thread, client_sock, fb_sock, std::ref(done));
        t1.join(); t2.join();
        closesocket(fb_sock);
        closesocket(client_sock);
        return;
    }

    std::string resp_headers;
    char rbuf[4096];
    while (resp_headers.find("\r\n\r\n") == std::string::npos) {
        fd_set fds; FD_ZERO(&fds); FD_SET(fb_sock, &fds);
        timeval tv{proxy_header_timeout_, 0};
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) break;
        int n = recv(fb_sock, rbuf, sizeof(rbuf) - 1, 0);
        if (n <= 0) break;
        rbuf[n] = 0;
        resp_headers.append(rbuf, n);
    }

    if (resp_headers.empty()) {
        closesocket(fb_sock); closesocket(client_sock); return;
    }

    std::string body_prefix;
    auto hdr_end_resp = resp_headers.find("\r\n\r\n");
    if (hdr_end_resp != std::string::npos) {
        body_prefix   = resp_headers.substr(hdr_end_resp + 4);
        resp_headers  = resp_headers.substr(0, hdr_end_resp + 4);
    }

    std::string cookie_hdr = "Set-Cookie: sara_token=" + echo_token +
                             "; Path=/; HttpOnly; SameSite=Lax\r\n";
    resp_headers.insert(resp_headers.size() - 2, cookie_hdr);

    ::send(client_sock, resp_headers.data(), (int)resp_headers.size(), 0);

    if (!body_prefix.empty())
        ::send(client_sock, body_prefix.data(), (int)body_prefix.size(), 0);

    std::atomic<bool> done{false};
    std::thread t1(pipe_thread, fb_sock, client_sock, std::ref(done));
    t1.join();

    closesocket(fb_sock);
    closesocket(client_sock);
}

// ─────────────────────────────────────────────────────────────────────────────
// EMBEDDED FRONTEND (fallback if no static_dir is set)
// The actual files in remote_runtime/frontend/ override these at runtime.
// ─────────────────────────────────────────────────────────────────────────────
std::string TerminalHttpServer::embedded_index_html() {
    return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1.0"/>
<title>SARA Terminal</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.css"/>
<link rel="stylesheet" href="/static/terminal.css"/>
</head>
<body>
  <div id="terminal-container">
  <div id="topbar">
    <span class="dot red"></span><span class="dot yellow"></span><span class="dot green"></span>
    <span id="session-label">SARA Remote Terminal</span>
    <span id="conn-status" class="disconnected">&#x2B24; Connecting...</span>
  </div>
  <div id="tab-bar"><div id="btn-add" title="New Terminal">+</div></div>
  <div id="terminal-area"></div>
  <div id="add-menu">
    <div class="menu-item" id="menu-new-tab">&#x2795; New Terminal Tab</div>
    <div class="menu-item" id="menu-split">&#x1F4D0; Split Current Terminal</div>
  </div>
  <div id="shell-picker">
    <div class="menu-title">Select Shell</div>
    <div class="menu-item" data-shell="powershell">&#x229E; PowerShell</div>
    <div class="menu-item" data-shell="cmd">&#x229E; CMD</div>
  </div>
</div>
<script src="https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-web-links@0.9.0/lib/xterm-addon-web-links.js"></script>
<script src="https://cdn.jsdelivr.net/npm/xterm-addon-unicode11@0.6.0/lib/xterm-addon-unicode11.js"></script>
<script src="/static/terminal.js"></script>
</body>
</html>)HTML";
}

std::string TerminalHttpServer::embedded_terminal_js() {
    return R"JS(
(function() {
  var params = new URLSearchParams(window.location.search);
  var token = params.get('token') || '';
  var pathParts = window.location.pathname.split('/');
  var sessionId = pathParts[pathParts.length - 1];
  var parentToken = token;
  var wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
  var fs = parseInt(params.get('fontSize'), 10);

  var THEMES = {
    vscode: {background:'#1e1e1e',foreground:'#d4d4d4',cursor:'#569cd6',selectionBackground:'#264f78',black:'#252526',red:'#d16969',green:'#6a9955',yellow:'#dcdcaa',blue:'#569cd6',magenta:'#c586c0',cyan:'#4ec9b0',white:'#d4d4d4',brightBlack:'#6e7681',brightRed:'#d16969',brightGreen:'#6a9955',brightYellow:'#dcdcaa',brightBlue:'#569cd6',brightMagenta:'#c586c0',brightCyan:'#4ec9b0',brightWhite:'#ffffff'},
    dracula: {background:'#282a36',foreground:'#f8f8f2',cursor:'#f8f8f2',selectionBackground:'#44475a',black:'#21222c',red:'#ff5555',green:'#50fa7b',yellow:'#f1fa8c',blue:'#bd93f9',magenta:'#ff79c6',cyan:'#8be9fd',white:'#f8f8f2',brightBlack:'#6272a4',brightRed:'#ff6e6e',brightGreen:'#69ff94',brightYellow:'#ffffa5',brightBlue:'#d6acff',brightMagenta:'#ff92df',brightCyan:'#a4ffff',brightWhite:'#ffffff'},
    classic: {background:'#0d1117',foreground:'#e6edf3',cursor:'#58a6ff',selectionBackground:'#388bfd33',black:'#484f58',red:'#ff7b72',green:'#3fb950',yellow:'#d29922',blue:'#58a6ff',magenta:'#bc8cff',cyan:'#39d353',white:'#b1bac4',brightBlack:'#6e7681',brightRed:'#ffa198',brightGreen:'#56d364',brightYellow:'#e3b341',brightBlue:'#79c0ff',brightMagenta:'#d2a8ff',brightCyan:'#56d364',brightWhite:'#f0f6fc'},
    windows: {background:'#0c0c0c',foreground:'#cccccc',cursor:'#cccccc',selectionBackground:'#76767666',black:'#0c0c0c',red:'#c50f1f',green:'#13a10e',yellow:'#c19c00',blue:'#0037da',magenta:'#881798',cyan:'#3a96dd',white:'#cccccc',brightBlack:'#767676',brightRed:'#e74856',brightGreen:'#16c60c',brightYellow:'#f9f1a5',brightBlue:'#3b78ff',brightMagenta:'#b4009e',brightCyan:'#61d6d6',brightWhite:'#f2f2f2'},
    hacker: {background:'#000000',foreground:'#00ff00',cursor:'#00ff00',selectionBackground:'#003300',black:'#000000',red:'#ff0000',green:'#00ff00',yellow:'#ffff00',blue:'#0000ff',magenta:'#ff00ff',cyan:'#00ffff',white:'#c0c0c0',brightBlack:'#808080',brightRed:'#ff0000',brightGreen:'#00ff00',brightYellow:'#ffff00',brightBlue:'#0000ff',brightMagenta:'#ff00ff',brightCyan:'#00ffff',brightWhite:'#ffffff'},
  };
  var themeName = (params.get('theme') || 'classic').replace(/-/g,'_');
  var theme = THEMES[themeName] || THEMES.classic;

  var bgEnabled = params.get('bgEnabled') === '1';
  var bgImage = params.get('bgImage') || '';
  var bgOpacity = parseFloat(params.get('bgOpacity')) || 0.3;

  var SHELLS = {
    powershell:{label:'PowerShell',cmd:'powershell.exe'},
    cmd:{label:'CMD',cmd:'cmd.exe'},
  };

  var tabBar = document.getElementById('tab-bar');
  var btnAdd = document.getElementById('btn-add');
  var addMenu = document.getElementById('add-menu');
  var shellPicker = document.getElementById('shell-picker');
  var termArea = document.getElementById('terminal-area');
  var statusEl = document.getElementById('conn-status');
  var sessionLabel = document.getElementById('session-label');

  var tabs = [];
  var activeIdx = 0;
  var splitMode = false;
  var splitLeftIdx = -1;
  var splitRightIdx = -1;
  var pendingAction = null;

  function createTermPane(idx) {
    var pane = document.createElement('div');
    pane.className = 'term-pane';
    pane.id = 'term-pane-' + idx;
    var termDiv = document.createElement('div');
    termDiv.className = 'terminal-el';
    termDiv.id = 'terminal-' + idx;
    pane.appendChild(termDiv);
    termArea.appendChild(pane);
    return pane;
  }

  function createTerminal(shellKey, sessToken, sessId, label) {
    var idx = tabs.length;
    var pane = createTermPane(idx);
    var termDiv = pane.querySelector('.terminal-el');
    var term = new Terminal({
      cursorBlink:true,
      fontFamily:'"Cascadia Code","JetBrains Mono","Fira Code","Consolas",monospace',
      fontSize:fs>0?fs:14,
      theme:theme,
      scrollback:5000,
      allowProposedApi:true,
    });
    var fitAddon = new FitAddon.FitAddon();
    term.loadAddon(fitAddon);
    term.loadAddon(new WebLinksAddon.WebLinksAddon());
    term.loadAddon(new Unicode11Addon.Unicode11Addon());
    term.unicode.activeVersion = '11';
    term.open(termDiv);

    var wsUrl = wsProto+'://'+location.host+'/ws/'+sessId+'?token='+sessToken;
    var tab = {
      id:sessId, token:sessToken, shell:shellKey,
      term:term, fitAddon:fitAddon,
      label:label||'Terminal', pane:pane,
      ws:null, retries:0, alive:true,
    };

    function connect() {
      if (!tab.alive) return;
      tab.ws = new WebSocket(wsUrl);
      tab.ws.binaryType = 'arraybuffer';
      tab.ws.onopen = function() {
        tab.retries = 0;
        if (tabs.indexOf(tab)===activeIdx) {
          statusEl.textContent = '\u2B24 Connected';
          statusEl.className = 'connected';
        }
        if (tab.ws.readyState===WebSocket.OPEN)
          tab.ws.send(JSON.stringify({type:'resize',cols:term.cols,rows:term.rows}));
      };
      tab.ws.onmessage = function(ev) {
        try {
          var msg = JSON.parse(ev.data);
          if (msg.type==='output') {
            var bin = atob(msg.data);
            var bytes = new Uint8Array(bin.length);
            for (var i=0;i<bin.length;i++) bytes[i]=bin.charCodeAt(i);
            term.write(bytes);
          } else if (msg.type==='exit') {
            var code = msg.code!==undefined?msg.code:'?';
            var reason = msg.reason||('Process exited (code '+code+')');
            term.write('\r\n\x1b[33m['+reason+']\x1b[0m\r\n');
            tab.alive = false;
            tab.label = tab.label + ' \u2716';
            renderTabBar();
            if (tabs.indexOf(tab)===activeIdx) {
              statusEl.textContent = '\u2B24 Ended';
              statusEl.className = 'disconnected';
            }
          } else if (msg.type==='error') {
            term.write('\r\n\x1b[31m[Error: '+(msg.reason||'Unknown')+']\x1b[0m\r\n');
          }
        } catch(e){}
      };
      tab.ws.onclose = function() {
        if (tabs.indexOf(tab)===activeIdx) {
          statusEl.textContent = '\u2B24 Disconnected';
          statusEl.className = 'disconnected';
        }
        if (tab.alive && tab.retries<3) {
          tab.retries++;
          setTimeout(connect, 2000);
        }
      };
    }

    term.onData(function(data) {
      if (tab.alive && tab.ws && tab.ws.readyState===WebSocket.OPEN)
        tab.ws.send(JSON.stringify({type:'input',data:btoa(data)}));
    });

    connect();
    tabs.push(tab);
    pane.addEventListener('click', function() {
      var i = tabs.indexOf(tab);
      if (i!==activeIdx) switchTab(i);
    });
    setTimeout(function(){try{fitAddon.fit();}catch(e){}}, 200);
    return tab;
  }

  function updateLayout() {
    for (var i=0;i<tabs.length;i++) {
      if (splitMode) {
        var vis = (i===splitLeftIdx||i===splitRightIdx);
        tabs[i].pane.classList.toggle('visible', vis);
        tabs[i].pane.classList.toggle('split-left', i===splitLeftIdx);
      } else {
        tabs[i].pane.classList.toggle('visible', i===activeIdx);
        tabs[i].pane.classList.remove('split-left');
      }
    }
    setTimeout(function(){
      for (var i=0;i<tabs.length;i++) {
        if (tabs[i].pane.classList.contains('visible'))
          try{tabs[i].fitAddon.fit();}catch(e){}
      }
    }, 50);
  }

  function renderTabBar() {
    var existing = tabBar.querySelectorAll('.tab');
    for (var i=0;i<existing.length;i++) existing[i].remove();
    for (var i=0;i<tabs.length;i++) {
      (function(idx){
        var el = document.createElement('div');
        el.className = 'tab'+(idx===activeIdx?' active':'');
        var lbl = document.createElement('span');
        lbl.className = 'tab-label';
        lbl.textContent = tabs[idx].label;
        el.appendChild(lbl);
        if (tabs.length>1) {
          var cls = document.createElement('span');
          cls.className = 'tab-close';
          cls.textContent = '\u00D7';
          cls.dataset.idx = idx;
          el.appendChild(cls);
        }
        el.addEventListener('click', function(e) {
          if (e.target.classList.contains('tab-close'))
            closeTab(parseInt(e.target.dataset.idx));
          else switchTab(idx);
        });
        tabBar.insertBefore(el, btnAdd);
      })(i);
    }
  }

  function switchTab(idx) {
    if (idx<0||idx>=tabs.length) return;
    activeIdx = idx;
    updateLayout();
    renderTabBar();
    if (tabs[idx].alive) {
      statusEl.textContent = '\u2B24 Connected';
      statusEl.className = 'connected';
    } else {
      statusEl.textContent = '\u2B24 Ended';
      statusEl.className = 'disconnected';
    }
    sessionLabel.textContent = tabs[idx].label;
    setTimeout(function(){tabs[idx].term.focus();}, 100);
  }

  function closeTab(idx) {
    if (tabs.length<=1||idx<0||idx>=tabs.length) return;
    var tab = tabs[idx];
    if (tab.ws) tab.ws.close();
    tab.term.dispose();
    tab.pane.remove();
    tabs.splice(idx, 1);
    if (splitMode) {
      if (idx===splitLeftIdx||idx===splitRightIdx) {
        splitMode = false; splitLeftIdx = -1; splitRightIdx = -1;
      } else {
        if (idx<splitLeftIdx) splitLeftIdx--;
        if (idx<splitRightIdx) splitRightIdx--;
      }
    }
    if (activeIdx>=tabs.length) activeIdx = tabs.length-1;
    if (splitMode && activeIdx!==splitLeftIdx && activeIdx!==splitRightIdx)
      activeIdx = splitLeftIdx;
    updateLayout();
    renderTabBar();
    if (tabs[activeIdx]) sessionLabel.textContent = tabs[activeIdx].label;
  }

  async function createNewSession(shellKey) {
    var shell = SHELLS[shellKey];
    if (!shell) return null;
    try {
      var url = '/api/new_session?parent_token='+encodeURIComponent(parentToken)+'&shell='+encodeURIComponent(shell.cmd);
      var resp = await fetch(url);
      var data = await resp.json();
      if (!data.session_id||!data.token) {
        console.error('sara: session create failed', data);
        return null;
      }
      return {id:data.session_id, token:data.token, shell:shellKey, label:shell.label};
    } catch(e) {
      console.error('sara: fetch failed', e);
      return null;
    }
  }

  async function handleNewTab(shellKey) {
    var sess = await createNewSession(shellKey);
    if (!sess) {
      termArea.querySelector('.term-pane.visible .terminal-el');
      var t = new Terminal({cursorBlink:true,theme:theme,fontSize:fs>0?fs:14,fontFamily:'monospace'});
      return;
    }
    createTerminal(sess.shell, sess.token, sess.id, sess.label);
    switchTab(tabs.length-1);
  }

  async function handleSplit(shellKey) {
    if (splitMode) return;
    var sess = await createNewSession(shellKey);
    if (!sess) return;
    createTerminal(sess.shell, sess.token, sess.id, sess.label);
    splitMode = true;
    splitLeftIdx = activeIdx;
    splitRightIdx = tabs.length-1;
    updateLayout();
    renderTabBar();
  }

  function positionMenu(menu, anchor) {
    var rect = anchor.getBoundingClientRect();
    menu.style.left = rect.left + 'px';
    menu.style.top = rect.bottom + 'px';
    menu.style.right = 'auto';
  }

  function showShellPicker(action) {
    pendingAction = action;
    addMenu.style.display = 'none';
    positionMenu(shellPicker, btnAdd);
    shellPicker.style.display = 'block';
  }

  function showAddMenu() {
    positionMenu(addMenu, btnAdd);
    addMenu.style.display = 'block';
    document.getElementById('menu-split').style.display = splitMode?'none':'block';
  }

  btnAdd.addEventListener('click', function(e) {
    e.stopPropagation();
    if (addMenu.style.display==='block') addMenu.style.display='none';
    else showAddMenu();
  });

  document.getElementById('menu-new-tab').addEventListener('click', function(e) {
    e.stopPropagation();
    showShellPicker('new-tab');
  });

  document.getElementById('menu-split').addEventListener('click', function(e) {
    e.stopPropagation();
    showShellPicker('split');
  });

  var pickerItems = shellPicker.querySelectorAll('.menu-item[data-shell]');
  for (var i=0;i<pickerItems.length;i++) {
    pickerItems[i].addEventListener('click', function(e) {
      e.stopPropagation();
      shellPicker.style.display = 'none';
      var shell = this.dataset.shell;
      if (pendingAction==='new-tab') handleNewTab(shell);
      else if (pendingAction==='split') handleSplit(shell);
      pendingAction = null;
    });
  }

  document.addEventListener('click', function() {
    addMenu.style.display = 'none';
    shellPicker.style.display = 'none';
  });

  if (bgEnabled && bgImage) {
    var container = document.getElementById('terminal-container');
    var bg = document.createElement('div');
    bg.id = 'term-bg';
    bg.style.cssText = 'position:fixed;inset:0;z-index:0;pointer-events:none;background:url("'+bgImage.replace(/"/g,'%22')+'") center/cover no-repeat;opacity:'+bgOpacity+';';
    container.insertBefore(bg, container.firstChild);
    var bgStyle = document.createElement('style');
    bgStyle.id = 'term-bg-style';
    bgStyle.textContent = '#terminal-area{background:transparent!important}.term-pane{background:transparent!important}.xterm,.xterm-viewport,.xterm-screen,.xterm-rows{background:transparent!important}';
    document.head.appendChild(bgStyle);
    var obs = new MutationObserver(function(){
      document.querySelectorAll('.xterm-viewport,.xterm-screen').forEach(function(el){el.style.background='transparent';});
    });
    obs.observe(termArea, {childList:true, subtree:true});
    setTimeout(function(){obs.disconnect();}, 3000);
  }

  createTerminal('powershell', token, sessionId, 'Terminal');
  renderTabBar();
  switchTab(0);

  var resizeObserver = new ResizeObserver(function(){
    for (var i=0;i<tabs.length;i++) {
      if (tabs[i].pane.classList.contains('visible'))
        try{tabs[i].fitAddon.fit();}catch(e){}
    }
  });
  resizeObserver.observe(termArea);

  setInterval(function(){
    for (var i=0;i<tabs.length;i++) {
      var w = tabs[i].ws;
      if (w&&w.readyState===WebSocket.OPEN)
        w.send(JSON.stringify({type:'ping'}));
    }
  }, 15000);

  window.saraShowAddMenu = function(){showAddMenu();};
})();
)JS";
}

std::string TerminalHttpServer::embedded_terminal_css() {
    return R"CSS(
*{margin:0;padding:0;box-sizing:border-box}
html,body{width:100%;height:100%;background:#0d1117;overflow:hidden;font-family:'Cascadia Code','JetBrains Mono',monospace}
#terminal-container{display:flex;flex-direction:column;width:100vw;height:100vh;background:#0d1117;position:relative}

#tab-bar{display:flex;align-items:center;background:#161b22;border-bottom:1px solid #30363d;min-height:34px;flex-shrink:0;overflow-x:auto;overflow-y:hidden;-webkit-overflow-scrolling:touch;scrollbar-width:none}
#tab-bar::-webkit-scrollbar{display:none}
.tab{display:flex;align-items:center;gap:6px;padding:0 14px;height:34px;background:transparent;border-right:1px solid #30363d;border-bottom:2px solid transparent;cursor:pointer;font-size:12px;color:#8b949e;white-space:nowrap;user-select:none;flex-shrink:0}
.tab:hover{background:#21262d}
.tab.active{background:#0d1117;color:#fff;border-bottom:2px solid #58a6ff}
.tab-label{max-width:120px;overflow:hidden;text-overflow:ellipsis}
.tab-close{font-size:15px;color:#484f58;padding:0 2px;line-height:1}
.tab-close:hover{color:#f85149}
#btn-add{padding:0 12px;height:34px;display:flex;align-items:center;cursor:pointer;color:#8b949e;font-size:18px;font-weight:bold;flex-shrink:0;border-right:1px solid #30363d}
#btn-add:hover{color:#fff;background:#21262d}

#topbar{display:flex;align-items:center;gap:8px;padding:6px 14px;background:#161b22;border-bottom:1px solid #30363d;height:30px;flex-shrink:0}
.dot{width:10px;height:10px;border-radius:50%;display:inline-block}
.dot.red{background:#ff5f56}.dot.yellow{background:#ffbd2e}.dot.green{background:#27c93f}
#session-label{color:#8b949e;font-size:11px;margin-left:8px;flex:1}
#conn-status{font-size:10px;font-family:monospace}
#conn-status.connected{color:#3fb950}#conn-status.disconnected{color:#f85149}

#terminal-area{flex:1;display:flex;flex-direction:row;overflow:hidden;position:relative}
.term-pane{display:none;flex:1;overflow:hidden;position:relative}
.term-pane.visible{display:block}
.term-pane.split-left{border-right:1px solid #30363d}
.terminal-el{width:100%;height:100%;overflow:hidden}
.xterm{height:100%!important;width:100%!important;padding:0!important}
.xterm-viewport{overflow:hidden!important}

#add-menu,#shell-picker{display:none;position:fixed;background:#161b22;border:1px solid #30363d;border-radius:8px;padding:4px 0;z-index:1000;min-width:200px;box-shadow:0 8px 24px rgba(0,0,0,.4)}
.menu-title{padding:8px 14px;font-size:11px;color:#8b949e;text-transform:uppercase;font-weight:600}
.menu-item{padding:10px 14px;font-size:13px;color:#c9d1d9;cursor:pointer;display:flex;align-items:center;gap:8px}
.menu-item:hover{background:#21262d;color:#fff}
)CSS";
}

// ───────────────────────────────────────────────────────────────────────────────
// DYNAMIC PORT REVERSE PROXY
// ───────────────────────────────────────────────────────────────────────────────
void TerminalHttpServer::proxy_to_port(SOCKET client_sock,
                                       const std::string& raw_request,
                                       int target_port,
                                       const std::string& prefix_to_strip) {
    SOCKET target_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (target_sock == INVALID_SOCKET) {
        send_http(client_sock, 502, "text/plain", "502 Bad Gateway");
        closesocket(client_sock); return;
    }

    sockaddr_in t_addr{};
    t_addr.sin_family      = AF_INET;
    t_addr.sin_addr.s_addr = htonl(0x7F000001); // 127.0.0.1
    t_addr.sin_port        = htons((u_short)target_port);

    if (connect(target_sock, (sockaddr*)&t_addr, sizeof(t_addr)) == SOCKET_ERROR) {
        closesocket(target_sock);
        
        target_sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (target_sock != INVALID_SOCKET) {
            sockaddr_in6 t_addr6{};
            t_addr6.sin6_family = AF_INET6;
            t_addr6.sin6_port = htons((u_short)target_port);
            inet_pton(AF_INET6, "::1", &t_addr6.sin6_addr);
            if (connect(target_sock, (sockaddr*)&t_addr6, sizeof(t_addr6)) == SOCKET_ERROR) {
                closesocket(target_sock);
                target_sock = INVALID_SOCKET;
            }
        }
        
        if (target_sock == INVALID_SOCKET) {
            send_http(client_sock, 503, "text/plain", "503 Service Unavailable");
            closesocket(client_sock); return;
        }
    }
    std::string fwd_req = raw_request;
    
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    };
    
    replace_all(fwd_req, "\r\nX-Forwarded-Host:", "\r\nX-Scrubbed-Host:");
    replace_all(fwd_req, "\r\nx-forwarded-host:", "\r\nX-Scrubbed-Host:");
    replace_all(fwd_req, "\r\nX-Forwarded-Proto:", "\r\nX-Scrubbed-Proto:");
    replace_all(fwd_req, "\r\nx-forwarded-proto:", "\r\nX-Scrubbed-Proto:");
    replace_all(fwd_req, "\r\nOrigin:", "\r\nX-Scrubbed-Origin:");
    replace_all(fwd_req, "\r\norigin:", "\r\nX-Scrubbed-Origin:");
    replace_all(fwd_req, "\r\nForwarded:", "\r\nX-Scrubbed-Forwarded:");
    replace_all(fwd_req, "\r\nforwarded:", "\r\nX-Scrubbed-Forwarded:");
    replace_all(fwd_req, "\r\nService-Worker: script", "\r\nX-Scrubbed-SW: script");
    replace_all(fwd_req, "\r\nservice-worker: script", "\r\nX-Scrubbed-SW: script");
    
    auto rewrite_host = [&]() {
        std::string lower_req = fwd_req;
        std::transform(lower_req.begin(), lower_req.end(), lower_req.begin(), ::tolower);
        auto pos = lower_req.find("\r\nhost:");
        if (pos != std::string::npos) {
            auto end = lower_req.find("\r\n", pos + 2);
            if (end != std::string::npos) {
                std::string new_host = "\r\nHost: localhost:" + std::to_string(target_port);
                fwd_req.replace(pos, end - pos, new_host);
            }
        }
    };
    rewrite_host();

    // Strip the prefix from the first line
    if (!prefix_to_strip.empty()) {
        auto line_end = fwd_req.find("\r\n");
        if (line_end != std::string::npos) {
            std::string req_line = fwd_req.substr(0, line_end);
            auto p1 = req_line.find(' ');
            if (p1 != std::string::npos) {
                auto p2 = req_line.find(' ', p1 + 1);
                if (p2 != std::string::npos) {
                    std::string url = req_line.substr(p1 + 1, p2 - p1 - 1);
                    if (url.find(prefix_to_strip) == 0) {
                        url = url.substr(prefix_to_strip.length());
                        if (url.empty() || url[0] != '/') url = "/" + url;
                        req_line = req_line.substr(0, p1 + 1) + url + req_line.substr(p2);
                        fwd_req = req_line + fwd_req.substr(line_end);
                    }
                }
            }
        }
    }

    int sent = 0;
    while (sent < (int)fwd_req.size()) {
        int n = ::send(target_sock, fwd_req.data() + sent, (int)(fwd_req.size() - sent), 0);
        if (n <= 0) break;
        sent += n;
    }

    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    
    auto pipe_thread = [](SOCKET from, SOCKET to, std::shared_ptr<std::atomic<bool>> done_flag) {
        char buf[16384];
        while (!(*done_flag)) {
            fd_set fds; FD_ZERO(&fds); FD_SET(from, &fds);
            timeval tv{1, 0};
            if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;
            int n = recv(from, buf, sizeof(buf), 0);
            if (n <= 0) { *done_flag = true; break; }
            int s = 0;
            while (s < n) {
                int r = ::send(to, buf + s, n - s, 0);
                if (r <= 0) { *done_flag = true; break; }
                s += r;
            }
        }
    };

    std::thread t1(pipe_thread, client_sock, target_sock, done);
    std::thread t2(pipe_thread, target_sock, client_sock, done);
    
    t1.join();
    t2.join();
    
    closesocket(target_sock);
    closesocket(client_sock);
}

} // namespace sara::remote
