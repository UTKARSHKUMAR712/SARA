#pragma once
#include <winsock2.h>
#include <string>

namespace sara::remote::api {

void handle_git_api_request(SOCKET sock, const std::string& req, const std::string& git_action, const std::string& workspace_path);

} // namespace sara::remote::api
