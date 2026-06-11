#include "../include/ApiFsController.h"
#include "../include/HttpUtils.h"
#include <filesystem>
#include <fstream>

using namespace sara::remote::http;

namespace sara::remote::api {

void handle_fs_api_request(SOCKET sock, const std::string& req, const std::string& action, const std::string& root_dir) {
    if (action.find("tree") == 0) {
        std::string req_path = action.substr(4);
        if (req_path.empty()) req_path = "/";
        
        std::string full = resolve_workspace_path(root_dir, req_path);
        
        std::string json = "{\"items\":[";
        bool first = true;
        std::error_code ec;
        if (std::filesystem::is_directory(full, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(full, ec)) {
                if (!first) json += ",";
                first = false;
                
                std::string name = entry.path().filename().string();
                bool isDir = entry.is_directory();
                std::string item_path = req_path;
                if (!item_path.empty() && item_path.back() != '/') item_path += "/";
                item_path += name;
                
                std::string escaped_name;
                for (char c : name) { if (c == '"' || c == '\\') escaped_name += '\\'; escaped_name += c; }
                
                json += "{\"name\":\"" + escaped_name + "\",\"path\":\"" + item_path + "\",\"isDir\":" + (isDir ? "true" : "false") + "}";
            }
        }
        json += "]}";
        send_http(sock, 200, "application/json; charset=utf-8", json);
    } else if (action.find("read") == 0) {
        std::string req_path = action.substr(4);
        std::string full = resolve_workspace_path(root_dir, req_path);
        
        std::ifstream f(full, std::ios::binary);
        if (!f) { send_404(sock); return; }
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        send_http(sock, 200, "text/plain; charset=utf-8", content);
    } else if (action.find("write") == 0) {
        std::string req_path = action.substr(5);
        std::string full = resolve_workspace_path(root_dir, req_path);
        
        auto body_pos = req.find("\r\n\r\n");
        std::string body;
        if (body_pos != std::string::npos) {
            body = req.substr(body_pos + 4);
        }
        
        auto cl_pos = req.find("Content-Length: ");
        if (cl_pos == std::string::npos) cl_pos = req.find("content-length: ");
        if (cl_pos != std::string::npos) {
            int cl = std::stoi(req.substr(cl_pos + 16));
            while (body.size() < cl) {
                char buf[4096];
                int n = recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) break;
                body.append(buf, n);
            }
        }
        
        std::ofstream f(full, std::ios::binary);
        if (f) {
            f << body;
            send_http(sock, 200, "application/json; charset=utf-8", "{}");
        } else {
            send_500(sock, "{\"error\":\"Failed to save\"}");
        }
    } else if (action.find("delete") == 0) {
        std::string req_path = action.substr(6);
        std::string full = resolve_workspace_path(root_dir, req_path);
        std::error_code ec;
        std::filesystem::remove_all(full, ec);
        if (ec) {
            send_500(sock, "{\"error\":\"" + ec.message() + "\"}");
        } else {
            send_http(sock, 200, "application/json", "{}");
        }
    } else if (action.find("mkdir") == 0) {
        std::string req_path = action.substr(5);
        std::string full = resolve_workspace_path(root_dir, req_path);
        std::error_code ec;
        std::filesystem::create_directories(full, ec);
        if (ec) {
            send_500(sock, "{\"error\":\"" + ec.message() + "\"}");
        } else {
            send_http(sock, 200, "application/json", "{}");
        }
    } else if (action.find("rename") == 0 || action.find("copy") == 0) {
        bool is_copy = action.find("copy") == 0;
        std::string req_path = action.substr(is_copy ? 4 : 6);
        
        std::string to_param;
        auto to_pos = req.find("to=");
        if (to_pos != std::string::npos) {
            auto amp = req.find('&', to_pos);
            auto qm = req.find('?', to_pos);
            auto space = req.find(' ', to_pos);
            
            auto end_pos = space;
            if (amp != std::string::npos && amp < end_pos) end_pos = amp;
            if (qm != std::string::npos && qm < end_pos) end_pos = qm;
            
            if (end_pos != std::string::npos) {
                to_param = req.substr(to_pos + 3, end_pos - to_pos - 3);
            } else {
                to_param = req.substr(to_pos + 3);
            }
            to_param = url_decode(to_param);
        }
        
        std::string full_from = resolve_workspace_path(root_dir, req_path);
        std::string full_to = resolve_workspace_path(root_dir, to_param);
        
        std::error_code ec;
        if (is_copy) {
            std::filesystem::copy(full_from, full_to, std::filesystem::copy_options::recursive, ec);
        } else {
            std::filesystem::rename(full_from, full_to, ec);
        }
        
        if (ec) {
            send_500(sock, "{\"error\":\"" + ec.message() + "\"}");
        } else {
            send_http(sock, 200, "application/json", "{}");
        }
    } else {
        send_404(sock);
    }
}

} // namespace sara::remote::api
