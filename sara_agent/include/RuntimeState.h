#pragma once
#include <string>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct RuntimeStats {
    double cpu_percent = 0;
    double ram_percent = 0;
    double ram_used_mb = 0;
    double ram_total_mb = 0;
    double gpu_percent = -1;
    int battery_percent = -1;
    bool on_ac_power = true;
    uint64_t uptime_seconds = 0;
    int process_count = 0;
    int active_tasks = 0;
    int ws_clients = 0;
};

class RuntimeState {
public:
    static RuntimeState& instance();

    void mark_startup();
    void mark_shutdown();

    uint64_t uptime() const;

    void set_telegram_ready(bool ready) { telegram_ready_ = ready; }
    bool is_telegram_ready() const { return telegram_ready_; }

    void set_websocket_ready(bool ready) { websocket_ready_ = ready; }
    bool is_websocket_ready() const { return websocket_ready_; }

    void set_active_tasks(int count) { active_tasks_ = count; }
    int active_tasks() const { return active_tasks_; }

    void set_ws_clients(int count) { ws_clients_ = count; }
    int ws_clients() const { return ws_clients_; }

    json get_summary() const;
    RuntimeStats get_stats() const;

    std::string format_status() const;

private:
    RuntimeState() = default;

    std::chrono::steady_clock::time_point start_time_;
    std::atomic<bool> telegram_ready_{false};
    std::atomic<bool> websocket_ready_{false};
    std::atomic<int> active_tasks_{0};
    std::atomic<int> ws_clients_{0};
};

} // namespace sara
