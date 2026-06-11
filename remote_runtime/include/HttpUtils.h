#pragma once
#include <winsock2.h>
#include <windows.h>
#include <string>
#include <vector>

namespace sara::remote::http {

std::string parse_path(const std::string& request);
std::string parse_query_param(const std::string& request, const std::string& param);
std::string parse_method(const std::string& request);
bool is_websocket_upgrade(const std::string& request);
bool do_ws_handshake(SOCKET sock, const std::string& request);

void send_http(SOCKET sock, int code, const std::string& content_type,
               const std::string& body, const std::string& extra_headers = "");
void send_404(SOCKET sock);
void send_403(SOCKET sock);
void send_500(SOCKET sock, const std::string& error_msg = "{\"error\":\"Internal Server Error\"}");

std::string url_decode(const std::string& in);
std::string resolve_workspace_path(const std::string& root_dir, const std::string& req_path);

} // namespace sara::remote::http
