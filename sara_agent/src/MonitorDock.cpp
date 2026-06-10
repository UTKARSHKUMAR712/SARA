#include "../include/MonitorDock.h"
#include "../include/TelegramGateway.h"
#include "../include/SystemMonitor.h"
#include "../include/Logger.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sstream>
#include <iomanip>
#include <thread>

extern sara::TelegramGateway g_telegram;

namespace sara {

DWORD g_top_pid = 0;
std::string g_top_name = "";

std::string MonitorDock::generate_bar(double percent, int length) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int filled = (percent / 100.0) * length;
    std::string bar = "[";
    for (int i = 0; i < length; ++i) {
        if (i < filled) bar += "█";
        else bar += "░";
    }
    bar += "]";
    return bar;
}

struct ProcessInfo {
    std::string name;
    DWORD pid;
    SIZE_T mem_bytes;
};

ProcessInfo get_top_process() {
    ProcessInfo top{"", 0, 0};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return top;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snap, &pe)) {
        do {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) {
                    if (pmc.WorkingSetSize > top.mem_bytes) {
                        top.mem_bytes = pmc.WorkingSetSize;
                        top.pid = pe.th32ProcessID;
                        std::wstring ws(pe.szExeFile);
                        top.name = std::string(ws.begin(), ws.end());
                    }
                }
                CloseHandle(h);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return top;
}

std::string MonitorDock::generate_text() {
    SystemMonitor mon;
    auto stats = mon.get_all();
    
    std::ostringstream oss;
    oss << "📊 **SYSTEM MONITOR**\n\n";
    
    oss << "CPU: " << generate_bar(stats.cpu_percent) << " " << (int)stats.cpu_percent << "%\n";
    
    oss << "RAM: " << generate_bar(stats.ram_percent) << " " << (int)stats.ram_percent 
        << "% (" << std::fixed << std::setprecision(1) << (stats.ram_used_mb / 1024.0) 
        << "/" << (stats.ram_total_mb / 1024.0) << " GB)\n";

    if (stats.battery_percent >= 0) {
        oss << "BAT: " << generate_bar(stats.battery_percent) << " " << stats.battery_percent 
            << "% " << (stats.on_ac_power ? "🔌" : "🔋") << "\n";
    }

    auto top = get_top_process();
    if (top.pid > 0) {
        g_top_pid = top.pid;
        g_top_name = top.name;
        oss << "\n⚠️ **TOP PROCESS**\n";
        oss << top.name << " (" << std::fixed << std::setprecision(1) << (top.mem_bytes / 1024.0 / 1024.0) << " MB)\n";
    }

    return oss.str();
}

json get_monitor_keyboard() {
    return json::array({
        json::array({
            {{"text", "🔄 Refresh"}, {"callback_data", "dock_mon:refresh"}},
            {{"text", "▶️ Auto-Refresh (15s)"}, {"callback_data", "dock_mon:auto_refresh"}}
        }),
        json::array({
            {{"text", "💀 Kill Top Process"}, {"callback_data", "dock_mon:kill_top"}}
        })
    });
}

void MonitorDock::send_dock(const std::string& chat_id) {
    g_telegram.send_inline_keyboard(chat_id, generate_text(), get_monitor_keyboard());
}

void MonitorDock::handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id) {
    if (action == "refresh") {
        g_telegram.edit_message_text(chat_id, message_id, generate_text(), get_monitor_keyboard());
        g_telegram.answer_callback_query(callback_query_id, "Refreshed");
    }
    else if (action == "auto_refresh") {
        g_telegram.answer_callback_query(callback_query_id, "Auto-Refresh Started");
        // Spawn a thread to avoid blocking the message handler
        std::thread([chat_id, message_id]() {
            // Loop 4 times, every 15s (1 minute total)
            for (int i = 0; i < 4; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(15));
                if (!g_telegram.is_running()) break;
                g_telegram.edit_message_text(chat_id, message_id, generate_text(), get_monitor_keyboard());
            }
        }).detach();
    }
    else if (action == "kill_top") {
        if (g_top_pid > 0 && g_top_pid != GetCurrentProcessId()) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, g_top_pid);
            if (h) {
                TerminateProcess(h, 0);
                CloseHandle(h);
                g_telegram.answer_callback_query(callback_query_id, "Killed " + g_top_name);
                g_telegram.edit_message_text(chat_id, message_id, generate_text(), get_monitor_keyboard());
            } else {
                g_telegram.answer_callback_query(callback_query_id, "Access Denied");
            }
        } else {
            g_telegram.answer_callback_query(callback_query_id, "Invalid Target");
        }
    }
    else if (action.find("kill_") == 0 && action != "kill_top") {
        DWORD pid = std::stoul(action.substr(5));
        if (pid > 0 && pid != GetCurrentProcessId()) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (h) {
                TerminateProcess(h, 0);
                CloseHandle(h);
                g_telegram.answer_callback_query(callback_query_id, "Process Killed!");
            } else {
                g_telegram.answer_callback_query(callback_query_id, "Access Denied");
            }
        } else {
            g_telegram.answer_callback_query(callback_query_id, "Invalid Target");
        }
    }
}

}
