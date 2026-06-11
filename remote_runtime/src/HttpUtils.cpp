#include "../include/HttpUtils.h"
#include <fstream>
#include <vector>
#include <wincrypt.h>

namespace sara::remote::http {

namespace {

std::string b64_enc(const unsigned char* data, size_t len) {
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

std::string sha1_b64(const std::string& input) {
    HCRYPTPROV p=0; HCRYPTHASH h=0; BYTE hash[20]; DWORD hl=20;
    CryptAcquireContext(&p,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);
    CryptCreateHash(p,CALG_SHA1,0,0,&h);
    CryptHashData(h,(BYTE*)input.data(),(DWORD)input.size(),0);
    CryptGetHashParam(h,HP_HASHVAL,hash,&hl,0);
    CryptDestroyHash(h); CryptReleaseContext(p,0);
    return b64_enc(hash,20);
}

} // namespace

std::string parse_path(const std::string& request) {
    auto space1 = request.find(' ');
    if (space1 == std::string::npos) return "/";
    auto space2 = request.find(' ', space1 + 1);
    std::string raw = (space2 != std::string::npos) ? request.substr(space1 + 1, space2 - space1 - 1) : request.substr(space1 + 1);
    auto q = raw.find('?'); return q != std::string::npos ? raw.substr(0, q) : raw;
}

std::string parse_query_param(const std::string& req, const std::string& param) {
    auto space1 = req.find(' ');
    if (space1 == std::string::npos) return "";
    auto space2 = req.find(' ', space1 + 1);
    std::string url = (space2 != std::string::npos) ? req.substr(space1 + 1, space2 - space1 - 1) : req.substr(space1 + 1);
    auto qp = url.find('?');
    if (qp == std::string::npos) return "";
    std::string qs = url.substr(qp + 1);
    auto kp = qs.find(param + "=");
    if (kp == std::string::npos) return "";
    kp += param.size() + 1;
    auto ep = qs.find('&', kp);
    return ep != std::string::npos ? qs.substr(kp, ep-kp) : qs.substr(kp);
}

std::string parse_method(const std::string& request) {
    auto space1 = request.find(' ');
    if (space1 == std::string::npos) return "";
    return request.substr(0, space1);
}

bool is_websocket_upgrade(const std::string& req) {
    std::string lower_req = req;
    for (char& c : lower_req) c = std::tolower(c);
    return lower_req.find("upgrade: websocket") != std::string::npos;
}

bool do_ws_handshake(SOCKET sock, const std::string& req) {
    std::string lower_req = req;
    for (char& c : lower_req) c = std::tolower(c);

    auto find_hdr = [&](const std::string& lower_name) {
        auto p = lower_req.find(lower_name); if (p == std::string::npos) return std::string{};
        p = lower_req.find(":", p); if (p == std::string::npos) return std::string{};
        p += 1; 
        while (p < lower_req.size() && lower_req[p] == ' ') p++;
        auto e = lower_req.find("\r\n", p);
        std::string val = req.substr(p, e-p);
        if (!val.empty() && val.back() == '\r') val.pop_back();
        return val;
    };
    std::string key = find_hdr("sec-websocket-key");
    if (key.empty()) return false;

    std::string accept = sha1_b64(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";
    return ::send(sock, resp.c_str(), (int)resp.size(), 0) != SOCKET_ERROR;
}

void send_http(SOCKET sock, int code, const std::string& ct, const std::string& body, const std::string& extra_headers) {
    std::string status;
    if (code == 200) status = "200 OK";
    else if (code == 400) status = "400 Bad Request";
    else if (code == 403) status = "403 Forbidden";
    else if (code == 404) status = "404 Not Found";
    else if (code == 500) status = "500 Internal Server Error";
    else status = std::to_string(code) + " Error";
    std::string resp =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Type: " + ct + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        + extra_headers +
        "Connection: close\r\n\r\n" + body;
    ::send(sock, resp.data(), (int)resp.size(), 0);
}

void send_404(SOCKET sock) {
    send_http(sock, 404, "text/plain", "404 Not Found");
}

void send_403(SOCKET sock) {
    send_http(sock, 403, "text/plain", "403 Forbidden — Invalid or expired session token");
}

void send_500(SOCKET sock, const std::string& error_msg) {
    send_http(sock, 500, "application/json", error_msg);
}

std::string url_decode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.length(); ++i) {
        if (in[i] == '%' && i + 2 < in.length()) {
            std::string hex = in.substr(i + 1, 2);
            out += (char)std::strtol(hex.c_str(), nullptr, 16);
            i += 2;
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}

std::string resolve_workspace_path(const std::string& root_dir, const std::string& req_path) {
    std::string full;
    if (req_path.length() >= 3 && req_path[0] == '/' && isalpha(req_path[1]) && req_path[2] == ':') {
        full = req_path.substr(1);
    } else if (req_path.length() >= 2 && isalpha(req_path[0]) && req_path[1] == ':') {
        full = req_path;
    } else if (req_path.find("/Users/") == 0) {
        full = "C:" + req_path;
    } else {
        full = root_dir + (req_path.empty() || req_path[0] != '/' ? "/" : "") + req_path;
    }
    for (char& c : full) if (c == '/') c = '\\';
    return full;
}

} // namespace sara::remote::http
