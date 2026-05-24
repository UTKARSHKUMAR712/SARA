#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

class WinAPIExecutor;
class SQLiteStore;
class ProcessMonitor;

// ── Trigger types ─────────────────────────────────────────────────────────────
// process_start   — when a process with trigger_value name starts
// process_stop    — when a process with trigger_value name stops
// cpu_high        — when CPU% exceeds trigger_value (e.g. "85")
// battery_low     — when battery% drops below trigger_value (e.g. "15")
// idle_detected   — when user is idle for trigger_value seconds
// network_change  — when network connectivity changes

struct EventRule {
    std::string id;
    std::string trigger_type;   // see above
    std::string trigger_value;  // process name OR numeric threshold
    json        actions;        // ordered array of { action, target, parameters }
    bool        enabled    = true;
    std::string description;
    long long   created_at = 0;
};

// ── Event Automation Engine ───────────────────────────────────────────────────
// Manages event rules and reacts to system events by executing action chains.
// Integrates with ProcessMonitor for process events; runs its own monitor
// loop for system-level triggers (CPU, battery, idle, network).
class EventAutomationEngine {
public:
    EventAutomationEngine();
    ~EventAutomationEngine();

    void start(WinAPIExecutor* executor, ProcessMonitor* process_monitor);
    void stop();
    bool is_running() const { return running_; }

    // Rule management
    bool                   add_rule(const EventRule& rule);
    bool                   remove_rule(const std::string& id);
    bool                   toggle_rule(const std::string& id, bool enabled);
    std::vector<EventRule> list_rules() const;
    EventRule*             find_rule(const std::string& id);

    void set_store(SQLiteStore* store);

    // Optional: send Telegram message when a rule fires
    void set_notify_callback(std::function<void(const std::string& msg)> cb);

    // Thresholds
    void set_cpu_threshold(double pct)     { cpu_threshold_ = pct; }
    void set_battery_threshold(int pct)    { battery_threshold_ = pct; }
    void set_idle_threshold(int seconds)   { idle_threshold_sec_ = seconds; }

private:
    void monitor_loop();
    void execute_rule_actions(const EventRule& rule, const std::string& trigger_info);
    void register_process_watches(ProcessMonitor* pm);
    void load_from_store();

    WinAPIExecutor* executor_       = nullptr;
    SQLiteStore*    store_          = nullptr;
    ProcessMonitor* process_monitor_ = nullptr;

    std::function<void(const std::string&)> notify_cb_;

    mutable std::mutex     mutex_;
    std::atomic<bool>      running_{false};
    std::thread            monitor_thread_;
    std::vector<EventRule> rules_;

    // Thresholds for system triggers
    double cpu_threshold_    = 85.0;
    int    battery_threshold_ = 15;
    int    idle_threshold_sec_ = 300;

    // State tracking for edge-triggered events
    bool   was_cpu_high_    = false;
    bool   was_battery_low_ = false;
    bool   was_idle_        = false;
    bool   was_connected_   = false;

    int monitor_interval_ms_ = 5000; // check system state every 5s
};

} // namespace sara
