#include "PortDetector.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <cstdint>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sara::remote {

std::vector<ActivePort> PortDetector::get_active_ports() {
    std::vector<ActivePort> active_ports;
    
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // First call to get the size needed
    if (GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<uint8_t> buffer(dwSize);
        PMIB_TCPTABLE_OWNER_PID pTcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

        if ((dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0)) == NO_ERROR) {
            for (int i = 0; i < (int)pTcpTable->dwNumEntries; i++) {
                if (pTcpTable->table[i].dwState == MIB_TCP_STATE_LISTEN) {
                    DWORD ip = ntohl(pTcpTable->table[i].dwLocalAddr);
                    
                    // Check if bound to 0.0.0.0 (0) or 127.0.0.1 (0x7F000001)
                    if (ip == 0 || ip == 0x7F000001) {
                        int port = ntohs((u_short)pTcpTable->table[i].dwLocalPort);
                        
                        // Ignore system ports (< 1024) and typical ephemeral ranges (> 49152)
                        // This leaves the user/registered port range 1024-49151
                        if (port > 1024 && port < 49152) {
                            // Filter out some known Windows background services that frequently listen
                            if (port == 3389 || // RDP
                                port == 5040 || // CDPUserSvc
                                port == 5357 || // WSDAPI
                                port == 7680 || // Delivery Optimization
                                port == 9080 || // SARA GUI IPC/WS
                                port == 9081 || // SARA Terminal
                                port == 9090)   // SARA FileBrowser
                            {
                                continue;
                            }
                            
                            // Prevent duplicates (if listening on both 0.0.0.0 and 127.0.0.1)
                            bool exists = false;
                            for (const auto& p : active_ports) {
                                if (p.port == port) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) {
                                active_ports.push_back({port, "Running"});
                            }
                        }
                    }
                }
            }
        }
    }
    
    // IPv6 support
    dwSize = 0;
    if (GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<uint8_t> buffer(dwSize);
        PMIB_TCP6TABLE_OWNER_PID pTcp6Table = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buffer.data());

        if (GetExtendedTcpTable(pTcp6Table, &dwSize, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (int i = 0; i < (int)pTcp6Table->dwNumEntries; i++) {
                if (pTcp6Table->table[i].dwState == MIB_TCP_STATE_LISTEN) {
                    int port = ntohs((u_short)pTcp6Table->table[i].dwLocalPort);
                    
                    if (port > 1024 && port < 49152) {
                        if (port == 3389 || port == 5040 || port == 5357 || port == 7680 || 
                            port == 9080 || port == 9081 || port == 9090) continue;
                        
                        bool exists = false;
                        for (const auto& p : active_ports) {
                            if (p.port == port) { exists = true; break; }
                        }
                        if (!exists) {
                            active_ports.push_back({port, "Running"});
                        }
                    }
                }
            }
        }
    }
    
    // Sort ascending
    std::sort(active_ports.begin(), active_ports.end(), [](const ActivePort& a, const ActivePort& b) {
        return a.port < b.port;
    });

    return active_ports;
}

} // namespace sara::remote
