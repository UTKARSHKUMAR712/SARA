#include "../include/ProcessMonitor.h"
#include "../include/Logger.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cctype>

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
ProcessMonitor::ProcessMonitor()  = default;
ProcessMonitor::~ProcessMonitor() { stop(); }

std::string ProcessMonitor::to_lower(const std::string& s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return out;
}

void ProcessMonitor::start() {
    if (running_) return;
    running_ = true;
    worker_  = std::thread(&ProcessMonitor::loop, this);
    Logger::instance().info("ProcessMonitor started");
}

void ProcessMonitor::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
    Logger::instance().info("ProcessMonitor stopped");
}

// ─────────────────────────────────────────────────────────────────────────────
void ProcessMonitor::on_process_start(const std::string& name, ProcessCallback cb) {
    std::string key = to_lower(name);
    std::lock_guard<std::mutex> lock(mutex_);
    watches_[key].process_name = key;
    watches_[key].on_start.push_back(std::move(cb));
    Logger::instance().info("ProcessMonitor: watching start of " + key);
}

void ProcessMonitor::on_process_stop(const std::string& name, ProcessCallback cb) {
    std::string key = to_lower(name);
    std::lock_guard<std::mutex> lock(mutex_);
    watches_[key].process_name = key;
    watches_[key].on_stop.push_back(std::move(cb));
    Logger::instance().info("ProcessMonitor: watching stop of " + key);
}

void ProcessMonitor::remove_watch(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    watches_.erase(to_lower(name));
}

bool ProcessMonitor::is_running_process(const std::string& name) const {
    std::string key = to_lower(name);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [pid, n] : known_pids_) {
        if (n == key) return true;
    }
    return false;
}

std::vector<std::string> ProcessMonitor::get_watched_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(watches_.size());
    for (auto& [k, _] : watches_) out.push_back(k);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
void ProcessMonitor::loop() {
    // Initial snapshot — don't fire "started" for already-running processes
    check_snapshot();
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms_));
        if (running_) check_snapshot();
    }
}

void ProcessMonitor::check_snapshot() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    // Build current pid → name map for watched processes only
    std::unordered_map<unsigned long, std::string> current_pids;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            // Convert wide exe name to lowercase narrow string
            char buf[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, buf, MAX_PATH, nullptr, nullptr);
            std::string name = to_lower(buf);

            std::lock_guard<std::mutex> lock(mutex_);
            if (watches_.count(name)) {
                current_pids[pe.th32ProcessID] = name;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    std::vector<std::pair<std::string, unsigned long>> started, stopped;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::unordered_map<std::string, int> old_counts;
        std::unordered_map<std::string, int> new_counts;

        for (auto& [pid, name] : known_pids_) old_counts[name]++;
        for (auto& [pid, name] : current_pids) new_counts[name]++;

        // Find newly started processes (transition from 0 to >0)
        for (auto& [name, count] : new_counts) {
            if (old_counts[name] == 0) {
                // Find one PID just to pass to callback
                unsigned long first_pid = 0;
                for (auto& [pid, n] : current_pids) if (n == name) { first_pid = pid; break; }
                started.emplace_back(name, first_pid);
            }
        }

        // Find stopped processes (transition from >0 to 0)
        for (auto& [name, count] : old_counts) {
            if (new_counts[name] == 0) {
                stopped.emplace_back(name, 0); // PID is gone anyway
            }
        }

        known_pids_ = current_pids;
    }

    // Fire callbacks outside the lock
    for (auto& [name, pid] : started) {
        Logger::instance().info("Process started: " + name + " [" + std::to_string(pid) + "]");
        std::vector<ProcessCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (watches_.count(name)) cbs = watches_[name].on_start;
        }
        for (auto& cb : cbs) {
            try { cb(name, pid); } catch (...) {}
        }
    }

    for (auto& [name, pid] : stopped) {
        Logger::instance().info("Process stopped: " + name + " [" + std::to_string(pid) + "]");
        std::vector<ProcessCallback> cbs;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (watches_.count(name)) cbs = watches_[name].on_stop;
        }
        for (auto& cb : cbs) {
            try { cb(name, pid); } catch (...) {}
        }
    }
}

} // namespace sara
