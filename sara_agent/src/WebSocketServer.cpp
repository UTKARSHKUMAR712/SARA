#include "../include/WebSocketServer.h"
#include "../include/Logger.h"
#include <winsock2.h>
#include <windows.h>
#include <wincrypt.h>
#include <sstream>
#include <chrono>
#include <functional>
#include <set>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace sara {

static const std::string WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = (unsigned int)data[i] << 16;
        if (i + 1 < len) v |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        out += b64[(v >> 18) & 0x3F];
        out += b64[(v >> 12) & 0x3F];
        out += (i + 1 < len) ? b64[(v >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? b64[v & 0x3F] : '=';
    }
    return out;
}

static std::string sha1(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hashLen = 20;

    CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    CryptHashData(hHash, (BYTE*)input.data(), (DWORD)input.size(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    return base64_encode(hash, hashLen);
}

WebSocketServer::WebSocketServer() = default;
WebSocketServer::~WebSocketServer() { stop(); }

bool WebSocketServer::start(int port) {
    if (running_) return true;
    port_ = port;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        Logger::instance().err("WSAStartup failed");
        return false;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        Logger::instance().err("WebSocket: socket creation failed");
        return false;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        Logger::instance().err("WebSocket: bind failed on port " + std::to_string(port));
        return false;
    }

    listen(sock, 10);

    running_ = true;
    accept_thread_ = std::thread([this, sock]() {
        Logger::instance().info("WebSocket server listening on port " + std::to_string(port_));
        while (running_) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            timeval tv{0, 500000};
            if (select(0, &readfds, nullptr, nullptr, &tv) <= 0) continue;

            sockaddr_in client_addr{};
            int addr_len = sizeof(client_addr);
            int client_sock = accept(sock, (sockaddr*)&client_addr, &addr_len);
            if (client_sock == INVALID_SOCKET) continue;

            u_long nonblock = 1;
            ioctlsocket(client_sock, FIONBIO, &nonblock);

            uint64_t cid = next_id_++;
            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_[cid] = {cid, "unknown", "", 0};
                client_sockets_[cid] = client_sock;
            }
            client_threads_[cid] = std::thread(handle_client, this, cid, client_sock);
            client_threads_[cid].detach();
        }
        closesocket(sock);
        WSACleanup();
    });
    accept_thread_.detach();
    return true;
}

void WebSocketServer::stop() {
    running_ = false;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& [cid, sock] : client_sockets_) {
            closesocket(sock);
        }
        client_sockets_.clear();
        clients_.clear();
        client_threads_.clear();
    }
    Logger::instance().info("WebSocket server stopped");
}

void WebSocketServer::handle_client(WebSocketServer* self, uint64_t cid, int sock) {
    auto& log = Logger::instance();
    char buf[4096];

    // Read HTTP upgrade request
    std::string request;
    int attempts = 0;
    while (attempts < 50) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = 0;
            request += buf;
            if (request.find("\r\n\r\n") != std::string::npos) break;
        } else if (n == 0 || (n < 0 && WSAGetLastError() != WSAEWOULDBLOCK)) {
            closesocket(sock);
            {
                std::lock_guard<std::mutex> lk(self->clients_mutex_);
                self->client_sockets_.erase(cid);
                self->clients_.erase(cid);
            }
            return;
        }
        Sleep(10);
        attempts++;
    }

    if (!ws_handshake(sock, request)) {
        log.err("WebSocket handshake failed for client " + std::to_string(cid));
        closesocket(sock);
        {
            std::lock_guard<std::mutex> lk(self->clients_mutex_);
            self->client_sockets_.erase(cid);
            self->clients_.erase(cid);
        }
        return;
    }

    // Extract client type from path or headers
    std::string client_type = "unknown";
    if (request.find("GET /plugin") != std::string::npos) client_type = "plugin";
    else if (request.find("GET /gui") != std::string::npos) client_type = "gui";

    {
        std::lock_guard<std::mutex> lk(self->clients_mutex_);
        if (self->clients_.count(cid)) {
            self->clients_[cid].type = client_type;
            self->clients_[cid].last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    log.info("WebSocket client connected: " + std::to_string(cid) + " type=" + client_type);

    if (self->conn_handler_) {
        self->conn_handler_(cid, client_type);
    }

    // Message loop
    u_long blocking = 0;
    ioctlsocket(sock, FIONBIO, &blocking);

    while (self->running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeval tv{2, 0};

        if (select(0, &readfds, nullptr, nullptr, &tv) <= 0) {
            // Check heartbeat timeout (30s)
            uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            uint64_t last;
            {
                std::lock_guard<std::mutex> lk(self->clients_mutex_);
                auto it = self->clients_.find(cid);
                if (it == self->clients_.end()) break;
                last = it->second.last_heartbeat;
            }
            if (now - last > 30) {
                log.info("WebSocket client timeout: " + std::to_string(cid));
                break;
            }
            continue;
        }

        std::string frame = ws_read_frame(sock);
        if (frame.empty()) break;

        // Update heartbeat
        {
            std::lock_guard<std::mutex> lk(self->clients_mutex_);
            auto it = self->clients_.find(cid);
            if (it != self->clients_.end()) {
                it->second.last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            }
        }

        // Parse JSON message
        try {
            auto msg = json::parse(frame);
            std::string type = msg.value("type", "");
            json payload = msg.value("payload", json::object());

            if (type == "ping") {
                ws_send_frame(sock, R"({"type":"pong"})");
                continue;
            }

            if (self->msg_handler_) {
                self->msg_handler_(cid, type, payload);
            }
        } catch (...) {
            log.warning("WebSocket: invalid JSON from " + std::to_string(cid));
        }
    }

    closesocket(sock);
    {
        std::lock_guard<std::mutex> lk(self->clients_mutex_);
        self->client_sockets_.erase(cid);
        self->client_threads_.erase(cid);
        self->clients_.erase(cid);
    }

    log.info("WebSocket client disconnected: " + std::to_string(cid));
    if (self->disc_handler_) {
        self->disc_handler_(cid);
    }
}

bool WebSocketServer::ws_handshake(int sock, const std::string& request) {
    auto find_header = [&](const std::string& name) -> std::string {
        auto pos = request.find(name);
        if (pos == std::string::npos) return "";
        pos = request.find(": ", pos);
        if (pos == std::string::npos) return "";
        pos += 2;
        auto end = request.find("\r\n", pos);
        return request.substr(pos, end - pos);
    };

    std::string key = find_header("Sec-WebSocket-Key");
    if (key.empty()) return false;

    std::string accept = sha1(key + WS_MAGIC);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";

    return ::send(sock, response.c_str(), (int)response.size(), 0) != SOCKET_ERROR;
}

bool WebSocketServer::ws_send_frame(int sock, const std::string& data) {
    std::vector<char> frame;
    frame.push_back(0x81); // FIN + text opcode

    size_t len = data.size();
    if (len < 126) {
        frame.push_back((char)len);
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((char)(len >> 8));
        frame.push_back((char)(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back((char)((len >> (i * 8)) & 0xFF));
        }
    }
    frame.insert(frame.end(), data.begin(), data.end());

    return ::send(sock, frame.data(), (int)frame.size(), 0) != SOCKET_ERROR;
}

std::string WebSocketServer::ws_read_frame(int sock) {
    char header[2];
    int n = recv(sock, header, 2, 0);
    if (n <= 0) return "";

    bool masked = (header[1] & 0x80);
    uint64_t len = header[1] & 0x7F;

    if (len == 126) {
        char ext[2];
        if (recv(sock, ext, 2, 0) <= 0) return "";
        len = ((unsigned char)ext[0] << 8) | (unsigned char)ext[1];
    } else if (len == 127) {
        char ext[8];
        if (recv(sock, ext, 8, 0) <= 0) return "";
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | (unsigned char)ext[i];
    }

    char mask[4] = {0};
    if (masked) {
        if (recv(sock, mask, 4, 0) <= 0) return "";
    }

    if (len > 1024 * 1024) return ""; // 1MB max frame

    std::vector<char> payload(len);
    uint64_t received = 0;
    while (received < len) {
        int n = recv(sock, payload.data() + received, (int)(len - received), 0);
        if (n <= 0) return "";
        received += n;
    }

    if (masked) {
        for (uint64_t i = 0; i < len; i++) {
            payload[i] ^= mask[i % 4];
        }
    }

    // Check for close frame
    if ((unsigned char)header[0] == 0x88) return "";

    return std::string(payload.data(), len);
}

bool WebSocketServer::send(uint64_t client_id, const json& payload) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = client_sockets_.find(client_id);
    if (it == client_sockets_.end()) return false;
    std::string data = payload.dump();
    return ws_send_frame(it->second, data);
}

bool WebSocketServer::broadcast(const json& payload, const std::string& type_filter) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::string data = payload.dump();
    bool ok = true;
    for (auto& [cid, sock] : client_sockets_) {
        if (!type_filter.empty() && clients_[cid].type != type_filter) continue;
        if (!ws_send_frame(sock, data)) ok = false;
    }
    return ok;
}

bool WebSocketServer::broadcast_to_type(const std::string& type, const json& payload) {
    return broadcast(payload, type);
}

std::vector<WSClient> WebSocketServer::get_clients() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::vector<WSClient> out;
    out.reserve(clients_.size());
    for (auto& [id, c] : clients_) out.push_back(c);
    return out;
}

int WebSocketServer::client_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return (int)clients_.size();
}

} // namespace sara
