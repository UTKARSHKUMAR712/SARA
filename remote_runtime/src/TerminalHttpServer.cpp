#include "../include/TerminalHttpServer.h"
#include "../include/TerminalSessionManager.h"
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

// ── SHA1 + Base64 for WS handshake ────────────────────────────────────────────
static std::string b64_enc(const unsigned char* data, size_t len) {
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
static std::string sha1_b64(const std::string& input) {
    HCRYPTPROV p=0; HCRYPTHASH h=0; BYTE hash[20]; DWORD hl=20;
    CryptAcquireContext(&p,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);
    CryptCreateHash(p,CALG_SHA1,0,0,&h);
    CryptHashData(h,(BYTE*)input.data(),(DWORD)input.size(),0);
    CryptGetHashParam(h,HP_HASHVAL,hash,&hl,0);
    CryptDestroyHash(h); CryptReleaseContext(p,0);
    return b64_enc(hash,20);
}

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

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
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
            int cs = accept(server_sock, (sockaddr*)&ca, &cl);
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

void TerminalHttpServer::handle_client(int sock) {
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

    // ── Health check ──────────────────────────────────────────────────────────
    if (path == "/health") {
        send_http(sock, 200, "text/plain", "ok"); closesocket(sock); return;
    }

    send_404(sock); closesocket(sock);
}

void TerminalHttpServer::handle_terminal_ws(int sock,
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

std::string TerminalHttpServer::parse_path(const std::string& req) {
    auto s = req.find("GET ");
    if (s == std::string::npos) return "/";
    s += 4;
    auto e = req.find(' ', s);
    std::string raw = (e != std::string::npos) ? req.substr(s, e-s) : req.substr(s);
    auto q = raw.find('?'); return q != std::string::npos ? raw.substr(0, q) : raw;
}

std::string TerminalHttpServer::parse_query_param(const std::string& req,
                                                   const std::string& param) {
    auto s = req.find("GET ");
    if (s == std::string::npos) return "";
    s += 4; auto e = req.find(' ', s);
    std::string url = (e != std::string::npos) ? req.substr(s, e-s) : req.substr(s);
    auto qp = url.find('?');
    if (qp == std::string::npos) return "";
    std::string qs = url.substr(qp + 1);
    auto kp = qs.find(param + "=");
    if (kp == std::string::npos) return "";
    kp += param.size() + 1;
    auto ep = qs.find('&', kp);
    return ep != std::string::npos ? qs.substr(kp, ep-kp) : qs.substr(kp);
}

bool TerminalHttpServer::is_websocket_upgrade(const std::string& req) {
    std::string lower_req = req;
    for (char& c : lower_req) c = std::tolower(c);
    return lower_req.find("upgrade: websocket") != std::string::npos;
}

bool TerminalHttpServer::do_ws_handshake(int sock, const std::string& req) {
    std::string lower_req = req;
    for (char& c : lower_req) c = std::tolower(c);

    auto find_hdr = [&](const std::string& lower_name) {
        auto p = lower_req.find(lower_name); if (p == std::string::npos) return std::string{};
        p = lower_req.find(":", p); if (p == std::string::npos) return std::string{};
        p += 1; 
        while (p < lower_req.size() && lower_req[p] == ' ') p++;
        auto e = lower_req.find("\r\n", p);
        // We must extract the key from the ORIGINAL req to preserve case if it matters, 
        // but base64 is case-sensitive! Sec-WebSocket-Key value is case sensitive!
        std::string val = req.substr(p, e-p);
        if (!val.empty() && val.back() == '\r') val.pop_back();
        return val;
    };
    std::string key = find_hdr("sec-websocket-key");
    if (key.empty()) return false;
    
    // DEBUG
    {
        std::ofstream dbg("http_debug.log", std::ios::app);
        dbg << "Extracted WS Key: [" << key << "]\n";
    }

    std::string accept = sha1_b64(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";
    return ::send(sock, resp.c_str(), (int)resp.size(), 0) != SOCKET_ERROR;
}

void TerminalHttpServer::send_http(int sock, int code, const std::string& ct,
                                    const std::string& body) {
    std::string status = (code == 200) ? "200 OK" : (code == 403) ? "403 Forbidden" : "404 Not Found";
    std::string resp =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + ct + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n" + body;
    ::send(sock, resp.data(), (int)resp.size(), 0);
}
void TerminalHttpServer::send_404(int sock) {
    send_http(sock, 404, "text/plain", "404 Not Found");
}
void TerminalHttpServer::send_403(int sock) {
    send_http(sock, 403, "text/plain", "403 Forbidden — Invalid or expired session token");
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
    <span id="conn-status" class="disconnected">⬤ Connecting...</span>
  </div>
  <div id="terminal"></div>
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
  // Parse URL params
  const params = new URLSearchParams(window.location.search);
  const token = params.get('token') || '';
  const pathParts = window.location.pathname.split('/');
  const sessionId = pathParts[pathParts.length - 1];

  // Determine WS URL
  const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
  const wsUrl = wsProto + '://' + location.host + '/ws/' + sessionId + '?token=' + token;

  // ── Theme system ──────────────────────────────────────────────────────────
  const THEMES = {
    vscode: {
      background: '#1e1e1e', foreground: '#d4d4d4', cursor: '#569cd6',
      selectionBackground: '#264f78',
      black:'#252526',red:'#d16969',green:'#6a9955',yellow:'#dcdcaa',
      blue:'#569cd6',magenta:'#c586c0',cyan:'#4ec9b0',white:'#d4d4d4',
      brightBlack:'#6e7681',brightRed:'#d16969',brightGreen:'#6a9955',
      brightYellow:'#dcdcaa',brightBlue:'#569cd6',brightMagenta:'#c586c0',
      brightCyan:'#4ec9b0',brightWhite:'#ffffff',
    },
    dracula: {
      background:'#282a36',foreground:'#f8f8f2',cursor:'#f8f8f2',
      selectionBackground:'#44475a',
      black:'#21222c',red:'#ff5555',green:'#50fa7b',yellow:'#f1fa8c',
      blue:'#bd93f9',magenta:'#ff79c6',cyan:'#8be9fd',white:'#f8f8f2',
      brightBlack:'#6272a4',brightRed:'#ff6e6e',brightGreen:'#69ff94',
      brightYellow:'#ffffa5',brightBlue:'#d6acff',brightMagenta:'#ff92df',
      brightCyan:'#a4ffff',brightWhite:'#ffffff',
    },
    classic: {
      background:'#0d1117',foreground:'#e6edf3',cursor:'#58a6ff',
      selectionBackground:'#388bfd33',
      black:'#484f58',red:'#ff7b72',green:'#3fb950',yellow:'#d29922',
      blue:'#58a6ff',magenta:'#bc8cff',cyan:'#39d353',white:'#b1bac4',
      brightBlack:'#6e7681',brightRed:'#ffa198',brightGreen:'#56d364',
      brightYellow:'#e3b341',brightBlue:'#79c0ff',brightMagenta:'#d2a8ff',
      brightCyan:'#56d364',brightWhite:'#f0f6fc',
    },
    windows: {
      background:'#0c0c0c',foreground:'#cccccc',cursor:'#cccccc',
      selectionBackground:'#76767666',
      black:'#0c0c0c',red:'#c50f1f',green:'#13a10e',yellow:'#c19c00',
      blue:'#0037da',magenta:'#881798',cyan:'#3a96dd',white:'#cccccc',
      brightBlack:'#767676',brightRed:'#e74856',brightGreen:'#16c60c',
      brightYellow:'#f9f1a5',brightBlue:'#3b78ff',brightMagenta:'#b4009e',
      brightCyan:'#61d6d6',brightWhite:'#f2f2f2',
    },
    hacker: {
      background:'#000000',foreground:'#00ff00',cursor:'#00ff00',
      selectionBackground:'#003300',
      black:'#000000',red:'#ff0000',green:'#00ff00',yellow:'#ffff00',
      blue:'#0000ff',magenta:'#ff00ff',cyan:'#00ffff',white:'#c0c0c0',
      brightBlack:'#808080',brightRed:'#ff0000',brightGreen:'#00ff00',
      brightYellow:'#ffff00',brightBlue:'#0000ff',brightMagenta:'#ff00ff',
      brightCyan:'#00ffff',brightWhite:'#ffffff',
    },
  };
  const themeName = (params.get('theme') || 'classic').replace(/-/g,'_');
  const t = THEMES[themeName] || THEMES.vscode;

  // ── Background image ──────────────────────────────────────────────────────
  const bgEnabled = params.get('bgEnabled') === '1';
  const bgImage = params.get('bgImage') || '';
  const bgOpacity = parseFloat(params.get('bgOpacity')) || 0.3;

  // xterm.js setup
  const fs = parseInt(params.get('fontSize'), 10);
  const term = new Terminal({
    cursorBlink: true,
    fontFamily: '"Cascadia Code", "JetBrains Mono", "Fira Code", "Consolas", monospace',
    fontSize: fs > 0 ? fs : 14,
    theme: t,
    scrollback: 5000,
    allowProposedApi: true,
  });

  const fitAddon = new FitAddon.FitAddon();
  const webLinksAddon = new WebLinksAddon.WebLinksAddon();
  const unicode11Addon = new Unicode11Addon.Unicode11Addon();
  term.loadAddon(fitAddon);
  term.loadAddon(webLinksAddon);
  term.loadAddon(unicode11Addon);
  term.unicode.activeVersion = '11';
  term.open(document.getElementById('terminal'));

  // ── Background image overlay ──────────────────────────────────────────────
  if (bgEnabled && bgImage) {
    const terms = document.getElementById('terminal-container');
    const bg = document.createElement('div');
    bg.id = 'term-bg';
    bg.style.cssText =
      'position:fixed;inset:0;z-index:0;pointer-events:none;' +
      'background:url("' + bgImage.replace(/"/g,'%22') + '") center/cover no-repeat;' +
      'opacity:' + bgOpacity + ';';
    terms.insertBefore(bg, terms.firstChild);

    const bgStyle = document.createElement('style');
    bgStyle.id = 'term-bg-style';
    bgStyle.textContent =
      '#terminal { background: transparent !important; }' +
      '#topbar { background: rgba(0,0,0,0.6) !important; }' +
      '.xterm, .xterm-viewport, .xterm-screen, .xterm-rows { background: transparent !important; }';
    document.head.appendChild(bgStyle);

    // Force xterm internal elements to transparent after it creates them
    const obs = new MutationObserver(() => {
      document.querySelectorAll('.xterm-viewport, .xterm-screen').forEach(el => {
        el.style.background = 'transparent';
      });
    });
    obs.observe(document.getElementById('terminal'), { childList: true, subtree: true });
    setTimeout(() => obs.disconnect(), 3000);
  }

  // Delay fit to ensure WebView has rendered dimensions
  function doFit() { try { fitAddon.fit(); } catch(e) {} }
  setTimeout(doFit, 100);
  setTimeout(doFit, 500);
  setTimeout(doFit, 1000);

  const statusEl = document.getElementById('conn-status');
  const sessionLabel = document.getElementById('session-label');
  sessionLabel.textContent = 'SARA Terminal — ' + sessionId;

  // WebSocket connection with reconnect
  let ws, retries = 0, MAX_RETRIES = 3;

  function connect() {
    statusEl.textContent = '⬤ Connecting...';
    statusEl.className = 'disconnected';
    ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
      retries = 0;
      statusEl.textContent = '⬤ Connected';
      statusEl.className = 'connected';
      // Send initial size
      sendResize();
    };

    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === 'output') {
          // base64 decode
          const bin = atob(msg.data);
          const bytes = new Uint8Array(bin.length);
          for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
          term.write(bytes);
        } else if (msg.type === 'exit') {
          statusEl.textContent = '⬤ Session Ended';
          statusEl.className = 'disconnected';
          const code = msg.code !== undefined ? msg.code : '?';
          const reason = msg.reason || ('Process exited (code ' + code + ')');
          term.write('\r\n\x1b[33m[' + reason + ']\x1b[0m\r\n');
        } else if (msg.type === 'error') {
          term.write('\r\n\x1b[31m[Error: ' + (msg.reason || 'Unknown') + ']\x1b[0m\r\n');
        }
      } catch(e) {}
    };

    ws.onerror = () => {};
    ws.onclose = () => {
      statusEl.textContent = '⬤ Disconnected';
      statusEl.className = 'disconnected';
      if (retries < MAX_RETRIES) {
        retries++;
        term.write('\r\n\x1b[33m[Reconnecting... attempt ' + retries + '/' + MAX_RETRIES + ']\x1b[0m\r\n');
        setTimeout(connect, 2000);
      } else {
        term.write('\r\n\x1b[31m[Connection lost. Session may have expired.]\x1b[0m\r\n');
      }
    };
  }

  function sendInput(data) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'input', data: btoa(data) }));
    }
  }

  function sendResize() {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'resize', cols: term.cols, rows: term.rows }));
    }
  }

  // Terminal input → WS
  term.onData(data => sendInput(data));

  // Window resize → PTY resize
  const resizeObserver = new ResizeObserver(() => {
    fitAddon.fit();
    sendResize();
  });
  resizeObserver.observe(document.getElementById('terminal'));

  window.addEventListener('resize', () => {
    fitAddon.fit();
    sendResize();
  });

  // Ping keepalive every 15s
  setInterval(() => {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'ping' }));
    }
  }, 15000);

  connect();
})();
)JS";
}

std::string TerminalHttpServer::embedded_terminal_css() {
    return R"CSS(
* { margin: 0; padding: 0; box-sizing: border-box; }

html, body {
  width: 100%; height: 100%;
  background: #0d1117;
  overflow: hidden;
  font-family: 'Cascadia Code', 'JetBrains Mono', monospace;
}

#terminal-container {
  display: flex;
  flex-direction: column;
  width: 100vw;
  height: 100vh;
  background: #0d1117;
}

#topbar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 6px 14px;
  background: #161b22;
  border-bottom: 1px solid #30363d;
  height: 34px;
  flex-shrink: 0;
}

.dot {
  width: 12px; height: 12px;
  border-radius: 50%;
  display: inline-block;
}
.dot.red    { background: #ff5f56; }
.dot.yellow { background: #ffbd2e; }
.dot.green  { background: #27c93f; }

#session-label {
  color: #8b949e;
  font-size: 12px;
  margin-left: 8px;
  flex: 1;
}

#conn-status {
  font-size: 11px;
  font-family: monospace;
}
#conn-status.connected    { color: #3fb950; }
#conn-status.disconnected { color: #f85149; }

#terminal {
  flex: 1;
  overflow: hidden;
  padding: 0;
}

.xterm { height: 100% !important; width: 100% !important; padding: 0 !important; }
.xterm-viewport { overflow: hidden !important; }
)CSS";
}

} // namespace sara::remote
