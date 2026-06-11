#include "../include/RuntimeState.h"
#include "../include/SystemMonitor.h"
#include <sstream>

#include "../include/NetworkMonitor.h"
#include <iomanip>

extern sara::NetworkMonitor g_net_monitor;

namespace sara {

RuntimeState& RuntimeState::instance() {
    static RuntimeState inst;
    return inst;
}

void RuntimeState::mark_startup() {
    start_time_ = std::chrono::steady_clock::now();
}

void RuntimeState::mark_shutdown() {
    // no-op
}

uint64_t RuntimeState::uptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
}

RuntimeStats RuntimeState::get_stats() const {
    RuntimeStats s;
    SystemMonitor mon;
    auto all = mon.get_all();
    s.cpu_percent = all.cpu_percent;
    s.ram_percent = all.ram_percent;
    s.ram_used_mb = all.ram_used_mb;
    s.ram_total_mb = all.ram_total_mb;
    s.gpu_percent = all.gpu_percent;
    s.battery_percent = all.battery_percent;
    s.on_ac_power = all.on_ac_power;
    s.uptime_seconds = uptime();
    s.process_count = all.process_count;
    s.active_tasks = active_tasks_;
    s.ws_clients = ws_clients_;
    return s;
}

json RuntimeState::get_summary() const {
    auto s = get_stats();
    json j;
    j["cpu_percent"] = s.cpu_percent;
    j["ram_percent"] = s.ram_percent;
    j["ram_used_mb"] = s.ram_used_mb;
    j["ram_total_mb"] = s.ram_total_mb;
    j["gpu_percent"] = s.gpu_percent;
    j["battery_percent"] = s.battery_percent;
    j["on_ac_power"] = s.on_ac_power;
    j["uptime_seconds"] = s.uptime_seconds;
    j["process_count"] = s.process_count;
    j["active_tasks"] = s.active_tasks;
    j["ws_clients"] = s.ws_clients;
    j["telegram_ready"] = telegram_ready_.load();
    j["websocket_ready"] = websocket_ready_.load();
    return j;
}

std::string RuntimeState::format_status() const {
    auto s = get_stats();
    SystemMonitor mon;
    std::ostringstream oss;
    oss << "🟢 SARA Runtime — Online\n"
        << "CPU: " << (int)s.cpu_percent << "%\n"
        << "RAM: " << (int)s.ram_percent << "% ("
        << (int)s.ram_used_mb << "/" << (int)s.ram_total_mb << " MB)\n";
    
    if (s.gpu_percent >= 0)
        oss << "GPU: " << (int)s.gpu_percent << "%\n";

    auto disks = mon.get_disk_info();
    for (const auto& disk : disks) {
        if (disk["drive"] == "C") {
            unsigned long long free_gb = disk["free_gb"];
            unsigned long long total_gb = disk["total_gb"];
            oss << "\nDisk C:  " << (total_gb - free_gb) << "/" << total_gb << " GB\n";
            break;
        }
    }

    auto net = g_net_monitor.get_status();
    oss << "\nNetwork\n";
    oss << "↓ " << std::fixed << std::setprecision(1) << (net.bytes_received_per_sec / (1024.0 * 1024.0)) << " MB/s\n";
    oss << "↑ " << std::fixed << std::setprecision(1) << (net.bytes_sent_per_sec / (1024.0 * 1024.0)) << " MB/s\n";

    if (s.battery_percent >= 0)
        oss << "\nBattery: " << s.battery_percent << "% "
            << (s.on_ac_power ? "(AC)" : "(BAT)") << "\n";
            
    oss << "\nUptime: " << (s.uptime_seconds / 3600) << "h "
        << ((s.uptime_seconds % 3600) / 60) << "m\n"
        << "Tasks: " << s.active_tasks
        << "  WS: " << s.ws_clients << " clients\n"
        << "Telegram: " << (telegram_ready_ ? "✅" : "❌");
    return oss.str();
}

} // namespace sara
