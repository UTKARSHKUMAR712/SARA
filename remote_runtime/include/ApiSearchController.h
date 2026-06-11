#pragma once
#include <winsock2.h>
#include <string>

namespace sara::remote::api {

void handle_search_api_request(SOCKET sock, const std::string& req, const std::string& path);

} // namespace sara::remote::api
