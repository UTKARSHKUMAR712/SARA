#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace sara {

using ProcessCallback = std::function<void(const std::string& name, unsigned long pid)>;

struct ProcessWatch {
    std::string                  process_name; // lowercase, e.g. "discord.exe"
    std::vector<ProcessCallback> on_start;
    std::vector<ProcessCallback> on_stop;
};

// ── Process Monitor ───────────────────────────────────────────────────────────
// Background thread that watches process start/stop events using ToolHelp32.
// Fires registered callbacks when a watched process appears or disappears.
// Lightweight: polls every 500 ms with minimal overhead.
class ProcessMonitor {
public:
    ProcessMonitor();
    ~ProcessMonitor();

    void start();
    void stop();
    bool is_running() const { return running_; }

    // Register a callback for when a specific process starts.
    // name is case-insensitive (e.g. "discord.exe" or "Discord.exe").
    void on_process_start(const std::string& name, ProcessCallback cb);

    // Register a callback for when a specific process stops.
    void on_process_stop(const std::string& name, ProcessCallback cb);

    // Remove all watches for a given process name.
    void remove_watch(const std::string& name);

    // Returns true if a process with the given name is currently running.
    bool is_running_process(const std::string& name) const;

    // Returns list of currently active watched process names.
    std::vector<std::string> get_watched_names() const;

    void set_check_interval_ms(int ms) { check_interval_ms_ = ms; }

private:
    void loop();
    void check_snapshot();
    std::string to_lower(const std::string& s) const;

    std::atomic<bool> running_{false};
    std::thread       worker_;
    mutable std::mutex mutex_;

    // watched processes: lowercase name → watch info
    std::unordered_map<std::string, ProcessWatch> watches_;

    // currently known processes: pid → lowercase name
    std::unordered_map<unsigned long, std::string> known_pids_;

    int check_interval_ms_ = 3000;
};

} // namespace sara
