#pragma once
#include <winsock2.h>
#include <string>

namespace sara::remote::api {

void handle_fs_api_request(SOCKET sock, const std::string& req, const std::string& action, const std::string& root_dir);

} // namespace sara::remote::api
