#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct NetworkStatus {
    bool        connected   = false;
    std::string adapter;
    std::string ip;
    std::string gateway;
    double      bytes_sent_per_sec     = 0.0;
    double      bytes_received_per_sec = 0.0;
};

// ── Network Monitor ───────────────────────────────────────────────────────────
// Polls network adapters periodically to detect connectivity changes and
// approximate bandwidth. Fires callbacks on connect/disconnect events.
class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();

    void start();
    void stop();
    bool is_running() const { return running_; }

    NetworkStatus get_status() const;
    json          get_status_json() const;

    void on_connect(std::function<void(const NetworkStatus&)> cb);
    void on_disconnect(std::function<void()> cb);

    void set_poll_interval_ms(int ms) { poll_interval_ms_ = ms; }

private:
    void poll_loop();
    NetworkStatus query_current() const;

    std::atomic<bool>  running_{false};
    std::thread        worker_;
    mutable std::mutex mutex_;

    NetworkStatus last_status_;
    int           poll_interval_ms_ = 5000;

    std::function<void(const NetworkStatus&)> on_connect_cb_;
    std::function<void()>                     on_disconnect_cb_;
};

} // namespace sara
