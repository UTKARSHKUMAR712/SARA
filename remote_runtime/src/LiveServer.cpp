#include "LiveServer.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>

namespace sara::remote {

namespace fs = std::filesystem;

static std::string get_mime_type(const std::string& ext) {
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain";
    return "application/octet-stream";
}

bool LiveServer::start(const std::string& directory, int port) {
    if (running_) stop();

    directory_ = directory;
    port_ = port;

    server_sock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock_ == INVALID_SOCKET) return false;

    int opt = 1;
    ::setsockopt(server_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Localhost only
    addr.sin_port = htons(port_);

    if (::bind(server_sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(server_sock_);
        return false;
    }

    if (::listen(server_sock_, SOMAXCONN) == SOCKET_ERROR) {
        ::closesocket(server_sock_);
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&LiveServer::accept_loop, this);
    return true;
}

void LiveServer::stop() {
    if (!running_) return;
    running_ = false;
    if (server_sock_ != -1) {
        ::closesocket(server_sock_);
        server_sock_ = -1;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void LiveServer::accept_loop() {
    while (running_) {
        int client_sock = ::accept(server_sock_, nullptr, nullptr);
        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        std::thread([this, client_sock]() {
            this->handle_client(client_sock);
        }).detach();
    }
}

void LiveServer::handle_client(int sock) {
    char buffer[4096];
    int bytes = ::recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        ::closesocket(sock);
        return;
    }
    buffer[bytes] = '\0';
    std::string request(buffer);

    // Parse first line (GET /path HTTP/1.1)
    size_t first_space = request.find(' ');
    size_t second_space = request.find(' ', first_space + 1);
    if (first_space != std::string::npos && second_space != std::string::npos) {
        std::string method = request.substr(0, first_space);
        std::string req_path = request.substr(first_space + 1, second_space - first_space - 1);

        if (method == "GET") {
            // Remove query string
            size_t qs = req_path.find('?');
            if (qs != std::string::npos) req_path = req_path.substr(0, qs);

            // Decode URL encoding
            std::string path;
            for (size_t i = 0; i < req_path.length(); ++i) {
                if (req_path[i] == '%' && i + 2 < req_path.length()) {
                    int value;
                    sscanf(req_path.substr(i + 1, 2).c_str(), "%x", &value);
                    path += static_cast<char>(value);
                    i += 2;
                } else if (req_path[i] == '+') {
                    path += ' ';
                } else {
                    path += req_path[i];
                }
            }

            if (path.empty() || path.back() == '/') {
                path += "index.html";
            }

            fs::path full_path = fs::path(directory_) / fs::path(path.substr(1)).make_preferred();
            
            // Basic security check to prevent directory traversal
            std::error_code ec;
            std::string canon_dir = fs::canonical(directory_, ec).string();
            std::string canon_target = fs::weakly_canonical(full_path, ec).string();

            if (canon_target.find(canon_dir) != 0) {
                std::string header = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
                ::send(sock, header.c_str(), header.size(), 0);
            } else if (fs::exists(canon_target) && fs::is_regular_file(canon_target)) {
                std::ifstream file(canon_target, std::ios::binary | std::ios::ate);
                if (file) {
                    auto size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    std::vector<char> content(size);
                    if (file.read(content.data(), size)) {
                        std::string ext = fs::path(canon_target).extension().string();
                        // to lowercase
                        for(auto& c : ext) c = tolower(c);

                        std::string mime = get_mime_type(ext);
                        std::ostringstream oss;
                        oss << "HTTP/1.1 200 OK\r\n"
                            << "Content-Type: " << mime << "\r\n"
                            << "Content-Length: " << size << "\r\n"
                            << "Access-Control-Allow-Origin: *\r\n"
                            << "Connection: close\r\n\r\n";
                        std::string header = oss.str();
                        ::send(sock, header.c_str(), header.size(), 0);
                        ::send(sock, content.data(), size, 0);
                    }
                } else {
                    std::string header = "HTTP/1.1 500 Internal Error\r\nConnection: close\r\n\r\n";
                    ::send(sock, header.c_str(), header.size(), 0);
                }
            } else {
                std::string header = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
                ::send(sock, header.c_str(), header.size(), 0);
            }
        }
    }

    ::closesocket(sock);
}

} // namespace sara::remote
