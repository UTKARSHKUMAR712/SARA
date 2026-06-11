#pragma once
#include <winsock2.h>
#include <string>

namespace sara::remote::api {

void handle_workspace_api_request(SOCKET sock, const std::string& req, const std::string& path, const std::string& action);

} // namespace sara::remote::api
