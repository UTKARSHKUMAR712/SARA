#include "../include/NetworkMonitor.h"
#include "../include/Logger.h"
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <chrono>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
NetworkMonitor::NetworkMonitor()  = default;
NetworkMonitor::~NetworkMonitor() { stop(); }

void NetworkMonitor::start() {
    if (running_) return;
    last_status_ = query_current();
    running_     = true;
    worker_      = std::thread(&NetworkMonitor::poll_loop, this);
    Logger::instance().info("NetworkMonitor started (connected="
        + std::string(last_status_.connected ? "yes" : "no") + ")");
}

void NetworkMonitor::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
}

void NetworkMonitor::on_connect(std::function<void(const NetworkStatus&)> cb) {
    on_connect_cb_ = std::move(cb);
}

void NetworkMonitor::on_disconnect(std::function<void()> cb) {
    on_disconnect_cb_ = std::move(cb);
}

// ─────────────────────────────────────────────────────────────────────────────
NetworkStatus NetworkMonitor::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_status_;
}

json NetworkMonitor::get_status_json() const {
    auto s = get_status();
    json j;
    j["connected"]              = s.connected;
    j["adapter"]                = s.adapter;
    j["ip"]                     = s.ip;
    j["bytes_sent_per_sec"]     = s.bytes_sent_per_sec;
    j["bytes_received_per_sec"] = s.bytes_received_per_sec;
    return j;
}

// ─────────────────────────────────────────────────────────────────────────────
NetworkStatus NetworkMonitor::query_current() const {
    NetworkStatus status;

    ULONG buf_len = 15000;
    auto* addrs   = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(buf_len));
    if (!addrs) return status;

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    DWORD ret   = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(addrs);
        addrs = static_cast<IP_ADAPTER_ADDRESSES*>(malloc(buf_len));
        if (!addrs) return status;
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &buf_len);
    }

    if (ret == NO_ERROR) {
        for (auto* a = addrs; a; a = a->Next) {
            if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (a->OperStatus != IfOperStatusUp)        continue;

            for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
                auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                std::string ip_str(ip);
                if (ip_str == "0.0.0.0" || ip_str.rfind("169.254.", 0) == 0) continue;

                // Convert adapter description
                int wlen = WideCharToMultiByte(CP_UTF8, 0, a->Description, -1,
                    nullptr, 0, nullptr, nullptr);
                std::string desc(wlen - 1, 0);
                if (wlen > 1) WideCharToMultiByte(CP_UTF8, 0, a->Description, -1,
                    &desc[0], wlen, nullptr, nullptr);

                status.connected = true;
                status.adapter   = desc;
                status.ip        = ip_str;
                break;
            }
            if (status.connected) break;
        }
    }
    free(addrs);
    return status;
}

// ─────────────────────────────────────────────────────────────────────────────
#include <netioapi.h>

void NetworkMonitor::poll_loop() {
    unsigned long long last_in = 0, last_out = 0;
    auto last_time = std::chrono::steady_clock::now();
    bool first_run = true;

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms_));
        if (!running_) break;

        NetworkStatus current = query_current();
        bool was_conn = false, now_conn = false;

        unsigned long long in_bytes = 0, out_bytes = 0;
        PMIB_IF_TABLE2 pIfTable = nullptr;
        if (GetIfTable2(&pIfTable) == NO_ERROR) {
            for (ULONG i = 0; i < pIfTable->NumEntries; i++) {
                if (pIfTable->Table[i].Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                in_bytes += pIfTable->Table[i].InOctets;
                out_bytes += pIfTable->Table[i].OutOctets;
            }
            FreeMibTable(pIfTable);
        }

        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_time).count();
        if (!first_run && dt > 0) {
            current.bytes_received_per_sec = (in_bytes >= last_in) ? (in_bytes - last_in) / dt : 0;
            current.bytes_sent_per_sec = (out_bytes >= last_out) ? (out_bytes - last_out) / dt : 0;
        }
        first_run = false;
        last_in = in_bytes;
        last_out = out_bytes;
        last_time = now;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            was_conn    = last_status_.connected;
            now_conn    = current.connected;
            last_status_ = current;
        }

        if (!was_conn && now_conn) {
            Logger::instance().info("NetworkMonitor: connected (" + current.ip + ")");
            if (on_connect_cb_) {
                try { on_connect_cb_(current); } catch (...) {}
            }
        } else if (was_conn && !now_conn) {
            Logger::instance().info("NetworkMonitor: disconnected");
            if (on_disconnect_cb_) {
                try { on_disconnect_cb_(); } catch (...) {}
            }
        }
    }
}

} // namespace sara
