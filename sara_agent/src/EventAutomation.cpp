#include "../include/EventAutomation.h"
#include "../include/ProcessMonitor.h"
#include "../include/SQLiteStore.h"
#include "../include/WinAPIExecutor.h"
#include "../include/SystemMonitor.h"
#include "../include/Logger.h"
#include <chrono>
#include <sstream>
#include <random>
#include <iomanip>

namespace sara {

static std::string gen_rule_id() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<unsigned long long> dis;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dis(gen);
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
EventAutomationEngine::EventAutomationEngine()  = default;
EventAutomationEngine::~EventAutomationEngine() { stop(); }

void EventAutomationEngine::set_store(SQLiteStore* store)  { store_ = store; }

void EventAutomationEngine::set_notify_callback(
    std::function<void(const std::string&)> cb) { notify_cb_ = std::move(cb); }

// ─────────────────────────────────────────────────────────────────────────────
void EventAutomationEngine::start(WinAPIExecutor* executor, ProcessMonitor* pm) {
    executor_        = executor;
    process_monitor_ = pm;

    if (store_) load_from_store();
    register_process_watches(pm);

    running_        = true;
    monitor_thread_ = std::thread(&EventAutomationEngine::monitor_loop, this);
    Logger::instance().info("EventAutomationEngine started ("
        + std::to_string(rules_.size()) + " rules loaded)");
}

void EventAutomationEngine::stop() {
    running_ = false;
    if (monitor_thread_.joinable()) monitor_thread_.join();
}

// ─────────────────────────────────────────────────────────────────────────────
bool EventAutomationEngine::add_rule(const EventRule& rule) {
    EventRule r = rule;
    if (r.id.empty()) r.id = gen_rule_id();
    if (r.created_at == 0) {
        r.created_at = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        rules_.push_back(r);
    }

    if (store_) store_->event_rule_add(r);

    // If it's a process watch rule, register the callback immediately
    if (process_monitor_
        && (r.trigger_type == "process_start" || r.trigger_type == "process_stop"))
    {
        std::string rule_id = r.id;
        auto self_cb = [this, rule_id](const std::string& name, unsigned long /*pid*/) {
            EventRule* found = find_rule(rule_id);
            if (found && found->enabled) {
                execute_rule_actions(*found, name);
            }
        };

        if (r.trigger_type == "process_start")
            process_monitor_->on_process_start(r.trigger_value, self_cb);
        else
            process_monitor_->on_process_stop(r.trigger_value, self_cb);
    }

    Logger::instance().info("EventRule added: [" + r.id.substr(0, 8) + "] "
        + r.trigger_type + ":" + r.trigger_value + " -> "
        + std::to_string(r.actions.size()) + " actions");
    return true;
}

bool EventAutomationEngine::remove_rule(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(rules_.begin(), rules_.end(),
        [&](const EventRule& r) { return r.id.find(id) == 0; }); // prefix match
    if (it == rules_.end()) return false;
    
    std::string full_id = it->id;
    rules_.erase(it);
    if (store_) store_->event_rule_remove(full_id);
    
    Logger::instance().info("EventRule removed: " + full_id.substr(0, 8));
    return true;
}

bool EventAutomationEngine::toggle_rule(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& r : rules_) {
        if (r.id == id) { r.enabled = enabled; return true; }
    }
    return false;
}

std::vector<EventRule> EventAutomationEngine::list_rules() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rules_;
}

EventRule* EventAutomationEngine::find_rule(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& r : rules_) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
void EventAutomationEngine::load_from_store() {
    if (!store_) return;
    rules_ = store_->event_rule_list();
    Logger::instance().info("Loaded " + std::to_string(rules_.size()) + " event rules from DB");
}

void EventAutomationEngine::register_process_watches(ProcessMonitor* pm) {
    if (!pm) return;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& rule : rules_) {
        if (!rule.enabled) continue;
        if (rule.trigger_type != "process_start" && rule.trigger_type != "process_stop") continue;

        std::string rule_id = rule.id;
        auto cb = [this, rule_id](const std::string& name, unsigned long) {
            EventRule* r = find_rule(rule_id);
            if (r && r->enabled) execute_rule_actions(*r, name);
        };

        if (rule.trigger_type == "process_start")
            pm->on_process_start(rule.trigger_value, cb);
        else
            pm->on_process_stop(rule.trigger_value, cb);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Background monitor loop for system-level triggers (CPU, battery, idle, network)
void EventAutomationEngine::monitor_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(monitor_interval_ms_));
        if (!running_) break;

        SystemMonitor mon;
        double cpu  = mon.get_cpu_usage();
        double ram  = mon.get_ram_usage();
        int    batt = mon.get_battery_percent();
        DWORD  idle = mon.get_idle_time_seconds();

        std::vector<EventRule> rules_copy;
        { std::lock_guard<std::mutex> lock(mutex_); rules_copy = rules_; }

        for (auto& rule : rules_copy) {
            if (!rule.enabled) continue;

            if (rule.trigger_type == "cpu_high") {
                double threshold = std::stod(rule.trigger_value.empty() ? "85" : rule.trigger_value);
                bool   now_high  = (cpu >= threshold);
                if (now_high && !was_cpu_high_) {
                    Logger::instance().info("EventRule fired: cpu_high (" + std::to_string((int)cpu) + "%)");
                    execute_rule_actions(rule, "CPU=" + std::to_string((int)cpu) + "%");
                }
                was_cpu_high_ = now_high;

            } else if (rule.trigger_type == "battery_low") {
                int threshold  = std::stoi(rule.trigger_value.empty() ? "15" : rule.trigger_value);
                bool now_low   = (batt >= 0 && batt <= threshold);
                if (now_low && !was_battery_low_) {
                    Logger::instance().info("EventRule fired: battery_low (" + std::to_string(batt) + "%)");
                    execute_rule_actions(rule, "Battery=" + std::to_string(batt) + "%");
                }
                was_battery_low_ = now_low;

            } else if (rule.trigger_type == "idle_detected") {
                int threshold   = std::stoi(rule.trigger_value.empty() ? "300" : rule.trigger_value);
                bool now_idle   = ((int)idle >= threshold);
                if (now_idle && !was_idle_) {
                    Logger::instance().info("EventRule fired: idle_detected (" + std::to_string(idle) + "s)");
                    execute_rule_actions(rule, "Idle=" + std::to_string(idle) + "s");
                }
                was_idle_ = now_idle;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void EventAutomationEngine::execute_rule_actions(
    const EventRule& rule, const std::string& trigger_info)
{
    if (!executor_) return;

    Logger::instance().info("Executing rule [" + rule.id.substr(0, 8) + "]: "
        + rule.description + " | trigger=" + trigger_info);

    if (notify_cb_) {
        notify_cb_("⚡ Rule fired: " + rule.description + "\nTrigger: " + trigger_info);
    }

    for (auto& step : rule.actions) {
        if (!step.contains("action")) continue;
        std::string act    = step.value("action", "");
        std::string target = step.value("target", "");
        json        params = step.value("parameters", json::object());

        if (!target.empty() && !params.contains("target"))
            params["target"] = target;

        int delay = step.value("delay_seconds", 0);
        if (delay > 0) {
            Logger::instance().info("Rule step: waiting " + std::to_string(delay) + "s before " + act);
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }

        auto result = executor_->execute(act, params);
        Logger::instance().info("Rule step [" + act + "]: "
            + (result.success ? "OK" : "FAIL") + " - " + result.message);

        // If step produced a file (screenshot/photo), send it via Telegram
        if (result.success && result.data.contains("path") && notify_cb_) {
            // notify_cb_ is used for text; file sending is handled by main.cpp
            // We embed the path in the notification so main can pick it up
            notify_cb_("__FILE__:" + result.data["path"].get<std::string>());
        }
    }
}

} // namespace sara
