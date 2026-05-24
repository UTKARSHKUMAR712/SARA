#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace sara {

using json = nlohmann::json;

struct SystemStats {
    double cpu_percent = 0;
    double ram_percent = 0;
    unsigned long long ram_total_mb = 0;
    unsigned long long ram_used_mb = 0;
    double disk_percent = 0;
    unsigned long long disk_total_gb = 0;
    unsigned long long disk_free_gb = 0;
    int process_count = 0;
    int battery_percent = -1;
    bool on_ac_power = true;
    double gpu_percent = -1.0;
    DWORD idle_seconds = 0;
};

class SystemMonitor {
public:
    SystemStats get_all();
    double get_cpu_usage();
    double get_ram_usage();
    double get_gpu_usage();
    DWORD get_idle_time_seconds();
    json get_disk_info();
    int get_process_count();
    json get_system_summary();
    int get_battery_percent();
};

}
