#include "../include/ApiWorkspaceController.h"
#include "../include/HttpUtils.h"
#include "../include/PortDetector.h"
#include "../include/LiveServer.h"
#include "../include/CloudflaredManager.h"
#include <iphlpapi.h>
#include <fstream>
#include <mutex>
#include <map>
#include <memory>

#pragma comment(lib, "iphlpapi.lib")

using namespace sara::remote::http;

namespace sara::remote::api {

namespace {
    std::mutex g_port_tunnels_mutex;
    std::map<int, std::unique_ptr<CloudflaredManager>> g_port_tunnels;
}

void handle_workspace_api_request(SOCKET sock, const std::string& req, const std::string& path, const std::string& action) {
    if (path == "/workspace/api/live_server/stop") {
        LiveServer::instance().stop();
        send_http(sock, 200, "application/json", "{\"status\":\"ok\"}");
    } else if (path.find("/workspace/api/live_server/start") == 0) {
        std::string dir = parse_query_param(req, "dir");
        dir = url_decode(dir);
        if (dir.empty()) dir = ".";
        bool ok = LiveServer::instance().start(dir, 5500);
        if (ok) {
            send_http(sock, 200, "application/json", "{\"status\":\"ok\", \"port\":5500}");
        } else {
            send_500(sock, "{\"error\":\"Failed to start live server\"}");
        }
    } else if (path.find("/workspace/api/ports/tunnel/") == 0) {
        int port = std::stoi(path.substr(28));
        
        std::string url;
        {
            std::lock_guard<std::mutex> lock(g_port_tunnels_mutex);
            auto it = g_port_tunnels.find(port);
            if (it != g_port_tunnels.end() && it->second->is_tunnel_alive()) {
                url = it->second->tunnel_url();
            } else {
                auto mgr = std::make_unique<CloudflaredManager>();
                mgr->start_quick_tunnel(port, [](const std::string&){} , false);
                g_port_tunnels[port] = std::move(mgr);
                
                for (int i = 0; i < 50; i++) {
                    url = g_port_tunnels[port]->tunnel_url();
                    if (!url.empty()) {
                        Sleep(3000);
                        break;
                    }
                    Sleep(200);
                }
            }
        }
        std::string json = "{\"url\":\"" + url + "\"}";
        send_http(sock, 200, "application/json", json);
    } else if (path.find("/workspace/api/ports/kill/") == 0) {
        int port = std::stoi(path.substr(26));
        
        {
            std::lock_guard<std::mutex> lock(g_port_tunnels_mutex);
            auto it = g_port_tunnels.find(port);
            if (it != g_port_tunnels.end()) {
                it->second->stop();
                g_port_tunnels.erase(it);
            }
        }
        
        int pid = -1;
        DWORD dwSize = 0;
        if (GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<uint8_t> buffer(dwSize);
            PMIB_TCPTABLE_OWNER_PID pTcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
            if (GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                for (int i = 0; i < (int)pTcpTable->dwNumEntries; i++) {
                    if (pTcpTable->table[i].dwState == MIB_TCP_STATE_LISTEN && ntohs((u_short)pTcpTable->table[i].dwLocalPort) == port) {
                        pid = pTcpTable->table[i].dwOwningPid; break;
                    }
                }
            }
        }
        if (pid == -1) {
            dwSize = 0;
            if (GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<uint8_t> buffer(dwSize);
                PMIB_TCP6TABLE_OWNER_PID pTcp6Table = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buffer.data());
                if (GetExtendedTcpTable(pTcp6Table, &dwSize, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                    for (int i = 0; i < (int)pTcp6Table->dwNumEntries; i++) {
                        if (pTcp6Table->table[i].dwState == MIB_TCP_STATE_LISTEN && ntohs((u_short)pTcp6Table->table[i].dwLocalPort) == port) {
                            pid = pTcp6Table->table[i].dwOwningPid; break;
                        }
                    }
                }
            }
        }
        if (pid > 0) {
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProc) { TerminateProcess(hProc, 0); CloseHandle(hProc); }
        }
        send_http(sock, 200, "application/json", "{\"status\":\"ok\"}");
    } else if (action == "ports") {
        auto ports = PortDetector::get_active_ports();
        std::string json = "[";
        for (size_t i = 0; i < ports.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"port\":" + std::to_string(ports[i].port) + ",\"status\":\"" + ports[i].status + "\"}";
        }
        json += "]";
        send_http(sock, 200, "application/json", json);
    } else if (action.find("settings") == 0) {
        std::string settings_file = "workspace_settings.json"; 
        if (req.find("GET") == 0) {
            std::ifstream f(settings_file);
            if (f) {
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                send_http(sock, 200, "application/json", content);
            } else {
                send_http(sock, 200, "application/json", "{}");
            }
        } else if (req.find("POST") == 0 || req.find("PUT") == 0) {
            auto body_pos = req.find("\r\n\r\n");
            std::string body = (body_pos != std::string::npos) ? req.substr(body_pos + 4) : "";
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
            std::ofstream f(settings_file);
            if (f) {
                f << body;
                send_http(sock, 200, "application/json", "{}");
            } else {
                send_500(sock, "{\"error\":\"Failed to save settings\"}");
            }
        }
    } else {
        send_404(sock);
    }
}

} // namespace sara::remote::api
