#include "spotify_ws.hpp"
#include "spotify_state.hpp"
#include "spotify_events.hpp"
#include "../../sara_agent/include/Logger.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

using json = nlohmann::json;

namespace sara {

static const uint16_t WS_PORT = 8765;

// ─── Singleton ───────────────────────────────────────────────────────────────

SpotifyWS& SpotifyWS::instance() {
    static SpotifyWS inst;
    return inst;
}

// ─── Start / Stop ────────────────────────────────────────────────────────────

void SpotifyWS::start() {
    if (running_) return;
    running_ = true;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    server_thread_ = std::thread(&SpotifyWS::server_loop, this);
    Logger::instance().info("SpotifyWS: server started on ws://127.0.0.1:8765");
}

void SpotifyWS::stop() {
    running_ = false;
    if (server_sock_ != INVALID_SOCKET) { closesocket(server_sock_); server_sock_ = INVALID_SOCKET; }
    if (client_sock_ != INVALID_SOCKET) { closesocket(client_sock_); client_sock_ = INVALID_SOCKET; }
    if (server_thread_.joinable()) server_thread_.join();
    WSACleanup();
}

bool SpotifyWS::is_client_connected() const {
    return client_connected_;
}

// ─── Server Loop ─────────────────────────────────────────────────────────────

void SpotifyWS::server_loop() {
    server_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock_ == INVALID_SOCKET) return;

    // Allow fast restart
    BOOL opt = TRUE;
    setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port        = htons(WS_PORT);

    if (bind(server_sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Logger::instance().err("SpotifyWS: bind failed on port 8765");
        closesocket(server_sock_);
        return;
    }
    listen(server_sock_, 1);

    while (running_) {
        SOCKET client = accept(server_sock_, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;

        // Only one client at a time (Spicetify)
        if (client_sock_ != INVALID_SOCKET) {
            closesocket(client_sock_);
            client_connected_ = false;
        }
        client_sock_ = client;

        if (client_thread_.joinable()) client_thread_.join();
        client_thread_ = std::thread(&SpotifyWS::client_loop, this, client);
    }
}

// ─── Client Loop ─────────────────────────────────────────────────────────────

void SpotifyWS::client_loop(SOCKET s) {
    if (!ws_handshake(s)) {
        closesocket(s);
        return;
    }

    client_connected_ = true;
    SpotifyStateManager::instance().set_connected(true);
    SpotifyEvents::instance().emit(SpotifyEventType::Connected,
                                  SpotifyStateManager::instance().get());
    Logger::instance().info("SpotifyWS: Spicetify client connected");

    while (running_ && client_connected_) {
        std::string frame = ws_recv(s);
        if (frame.empty()) break;
        process_frame(frame);
    }

    client_connected_ = false;
    SpotifyStateManager::instance().set_connected(false);
    SpotifyEvents::instance().emit(SpotifyEventType::Disconnected,
                                  SpotifyStateManager::instance().get());
    Logger::instance().info("SpotifyWS: Spicetify client disconnected");
    closesocket(s);
    if (client_sock_ == s) client_sock_ = INVALID_SOCKET;
}

// ─── WebSocket Handshake ─────────────────────────────────────────────────────

std::string SpotifyWS::base64_encode(const unsigned char* data, size_t len) {
    DWORD out_len = 0;
    CryptBinaryToStringA(data, (DWORD)len,
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &out_len);
    std::string out(out_len, '\0');
    CryptBinaryToStringA(data, (DWORD)len,
        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &out_len);
    out.resize(out_len);
    return out;
}

std::string SpotifyWS::sha1_b64(const std::string& input) {
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    CryptAcquireContextA(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash);
    CryptHashData(hash, (BYTE*)input.data(), (DWORD)input.size(), 0);
    BYTE buf[20];
    DWORD buf_len = 20;
    CryptGetHashParam(hash, HP_HASHVAL, buf, &buf_len, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return base64_encode(buf, 20);
}

bool SpotifyWS::ws_handshake(SOCKET s) {
    char buf[4096] = {};
    int n = recv(s, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;

    std::string req(buf, n);
    std::string key;
    auto pos = req.find("Sec-WebSocket-Key:");
    if (pos == std::string::npos) return false;
    pos += 18;
    while (pos < req.size() && req[pos] == ' ') pos++;
    auto end = req.find("\r\n", pos);
    key = req.substr(pos, end - pos);

    std::string magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept = sha1_b64(magic);

    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

    send(s, resp.c_str(), (int)resp.size(), 0);
    return true;
}

// ─── WebSocket Frame I/O ─────────────────────────────────────────────────────

bool SpotifyWS::ws_send(SOCKET s, const std::string& payload) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + text opcode

    size_t len = payload.size();
    if (len < 126) {
        frame.push_back((uint8_t)len);
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--)
            frame.push_back((len >> (8 * i)) & 0xFF);
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return send(s, (char*)frame.data(), (int)frame.size(), 0) > 0;
}

std::string SpotifyWS::ws_recv(SOCKET s) {
    uint8_t header[2];
    if (recv(s, (char*)header, 2, MSG_WAITALL) != 2) return {};

    bool masked  = (header[1] & 0x80) != 0;
    uint64_t len = header[1] & 0x7F;

    if (len == 126) {
        uint8_t ext[2];
        if (recv(s, (char*)ext, 2, MSG_WAITALL) != 2) return {};
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        uint8_t ext[8];
        if (recv(s, (char*)ext, 8, MSG_WAITALL) != 8) return {};
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }

    uint8_t mask[4] = {};
    if (masked) {
        if (recv(s, (char*)mask, 4, MSG_WAITALL) != 4) return {};
    }

    std::string data(len, '\0');
    size_t got = 0;
    while (got < len) {
        int r = recv(s, &data[got], (int)(len - got), 0);
        if (r <= 0) return {};
        got += r;
    }
    if (masked)
        for (size_t i = 0; i < len; i++) data[i] ^= mask[i % 4];

    return data;
}

// ─── Send Command ─────────────────────────────────────────────────────────────

bool SpotifyWS::send_command(const json& cmd) {
    if (!client_connected_ || client_sock_ == INVALID_SOCKET) return false;
    std::lock_guard<std::mutex> lk(send_mtx_);
    return ws_send(client_sock_, cmd.dump());
}

// ─── Metadata Parser ─────────────────────────────────────────────────────────

void SpotifyWS::process_frame(const std::string& payload) {
    try {
        auto j = json::parse(payload);
        if (j.value("type", "") != "metadata") return;

        auto& d = j["data"];
        SpotifyState prev = SpotifyStateManager::instance().get();

        SpotifyState s;
        s.title       = d.value("title",    "");
        s.artist      = d.value("artist",   "");
        s.album       = d.value("album",    "");
        s.image       = d.value("image",    "");
        if (d.contains("next_tracks") && d["next_tracks"].is_array()) {
            for (auto& t : d["next_tracks"]) s.next_tracks.push_back(t.get<std::string>());
        }
        s.duration_ms = d.value("duration", 0);
        s.progress_ms = d.value("progress", 0);
        s.volume      = d.value("volume",   0);
        s.playing     = d.value("playing",  false);
        s.shuffle     = d.value("shuffle",  false);
        s.repeat_mode = d.value("repeat",   0);
        s.hearted     = d.value("heart",    false);
        s.connected   = true;
        SpotifyStateManager::instance().set(s);

        auto& ev = SpotifyEvents::instance();

        // Fire specific events for dock / automation
        if (s.title   != prev.title)   ev.emit(SpotifyEventType::SongChanged,    s);
        if (s.playing  != prev.playing) ev.emit(s.playing ? SpotifyEventType::Resumed : SpotifyEventType::Paused, s);
        if (s.volume   != prev.volume)  ev.emit(SpotifyEventType::VolumeChanged,  s);
        if (s.shuffle  != prev.shuffle) ev.emit(SpotifyEventType::ShuffleChanged, s);
        if (s.repeat_mode != prev.repeat_mode) ev.emit(SpotifyEventType::RepeatChanged, s);
        if (s.hearted  != prev.hearted) ev.emit(SpotifyEventType::HeartChanged,   s);

        ev.emit(SpotifyEventType::MetadataUpdate, s);

    } catch (...) {}
}

} // namespace sara
