#include "../include/SystemMonitor.h"
#include <windows.h>
#include <pdh.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <thread>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

namespace sara {

double SystemMonitor::get_cpu_usage() {
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool init = false;

    if (!init) {
        if (PdhOpenQueryA(nullptr, 0, &query) == ERROR_SUCCESS) {
            PdhAddEnglishCounterA(query,
                "\\Processor(_Total)\\% Processor Time", 0, &counter);
            PdhCollectQueryData(query);
            init = true;
        }
    }

    if (!query || !counter) return 0.0;

    PdhCollectQueryData(query);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    PdhCollectQueryData(query);

    PDH_FMT_COUNTERVALUE value;
    PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value);
    return value.doubleValue;
}

double SystemMonitor::get_ram_usage() {
    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (!GlobalMemoryStatusEx(&mem)) return 0.0;
    return static_cast<double>(mem.dwMemoryLoad);
}

json SystemMonitor::get_disk_info() {
    json disks = json::array();
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char root[] = { static_cast<char>('A' + i), ':', '\\', 0 };
            ULARGE_INTEGER total, free;
            if (GetDiskFreeSpaceExA(root, nullptr, &total, &free)) {
                disks.push_back({
                    {"drive", std::string(1, root[0])},
                    {"total_gb", static_cast<unsigned long long>(
                        total.QuadPart / (1024 * 1024 * 1024))},
                    {"free_gb", static_cast<unsigned long long>(
                        free.QuadPart / (1024 * 1024 * 1024))}
                });
            }
        }
    }
    return disks;
}

int SystemMonitor::get_process_count() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int count = 0;
    if (Process32FirstW(snap, &pe)) {
        do { count++; } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return count;
}

int SystemMonitor::get_battery_percent() {
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return -1;
    if (sps.BatteryFlag == 128 || sps.BatteryFlag == 255) return -1;
    return sps.BatteryLifePercent;
}

SystemStats SystemMonitor::get_all() {
    SystemStats stats;
    stats.cpu_percent    = get_cpu_usage();
    stats.ram_percent    = get_ram_usage();
    stats.gpu_percent    = get_gpu_usage();
    stats.idle_seconds   = get_idle_time_seconds();

    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        stats.battery_percent = (sps.BatteryFlag == 128 || sps.BatteryFlag == 255)
            ? -1 : sps.BatteryLifePercent;
        stats.on_ac_power = (sps.ACLineStatus == 1);
    }

    MEMORYSTATUSEX mem = { sizeof(mem) };
    if (GlobalMemoryStatusEx(&mem)) {
        stats.ram_total_mb = mem.ullTotalPhys / (1024 * 1024);
        stats.ram_used_mb  = (mem.ullTotalPhys - mem.ullAvailPhys) / (1024 * 1024);
    }

    stats.process_count = get_process_count();
    return stats;
}


json SystemMonitor::get_system_summary() {
    auto stats = get_all();
    json j;
    j["cpu_percent"]    = stats.cpu_percent;
    j["ram_percent"]    = stats.ram_percent;
    j["ram_used_mb"]    = stats.ram_used_mb;
    j["ram_total_mb"]   = stats.ram_total_mb;
    j["process_count"]  = stats.process_count;
    j["idle_seconds"]   = stats.idle_seconds;
    j["disks"]          = get_disk_info();
    if (stats.gpu_percent >= 0)
        j["gpu_percent"] = stats.gpu_percent;
    if (stats.battery_percent >= 0) {
        j["battery_percent"] = stats.battery_percent;
        j["on_ac_power"]     = stats.on_ac_power;
    }
    return j;
}

// ─────────────────────────────────────────────────────────────────────────────
double SystemMonitor::get_gpu_usage() {
    static PDH_HQUERY   query   = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool         init    = false;
    static bool         avail   = true;

    if (!avail) return -1.0;

    if (!init) {
        if (PdhOpenQueryA(nullptr, 0, &query) == ERROR_SUCCESS) {
            // This counter is available on Win10+ with dedicated GPU
            DWORD res = PdhAddEnglishCounterA(query,
                "\\GPU Engine(*engtype_3D)\\Utilization Percentage",
                0, &counter);
            if (res != ERROR_SUCCESS) {
                // Try broader GPU counter
                res = PdhAddEnglishCounterA(query,
                    "\\GPU Engine(*)\\Utilization Percentage",
                    0, &counter);
            }
            if (res == ERROR_SUCCESS) {
                PdhCollectQueryData(query);
                init = true;
            } else {
                avail = false;
                return -1.0;
            }
        } else {
            avail = false;
            return -1.0;
        }
    }

    if (!query || !counter) return -1.0;

    PdhCollectQueryData(query);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    PdhCollectQueryData(query);

    PDH_FMT_COUNTERVALUE val;
    DWORD fmt = PDH_FMT_DOUBLE | PDH_FMT_NOCAP100;
    if (PdhGetFormattedCounterValue(counter, fmt, nullptr, &val) != ERROR_SUCCESS)
        return -1.0;
    return std::min(val.doubleValue, 100.0);
}

// ─────────────────────────────────────────────────────────────────────────────
DWORD SystemMonitor::get_idle_time_seconds() {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    if (!GetLastInputInfo(&lii)) return 0;
    DWORD elapsed_ms = GetTickCount() - lii.dwTime;
    return elapsed_ms / 1000;
}

} // namespace sara
