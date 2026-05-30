#include "../include/Scheduler.h"
#include "../include/Logger.h"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <random>
#include <iomanip>

std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<unsigned long long> dis;
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << dis(gen)
        << std::setw(16) << std::setfill('0') << dis(gen);
    return oss.str();
}

namespace sara {

json Task::to_json() const {
    json j;
    j["id"] = id;
    j["source"] = source;
    j["source_id"] = source_id;
    j["type"] = type;
    j["action"] = action;
    if (!target.empty()) j["target"] = target;
    if (!parameters.empty()) j["parameters"] = parameters;
    j["requires_approval"] = requires_approval;
    j["created_at"] = created_at;
    j["execute_at"] = execute_at;
    j["repeat"] = repeat;
    j["interval_seconds"] = interval_seconds;
    if (!cron_expression.empty()) j["cron_expression"] = cron_expression;
    if (!heartbeat_check_id.empty()) j["heartbeat_check_id"] = heartbeat_check_id;
    j["priority"] = priority;
    j["status"] = status;
    return j;
}

Task Task::from_json(const json& j) {
    Task t;
    t.id = j.value("id", "");
    t.source = j.value("source", "");
    t.source_id = j.value("source_id", "");
    t.type = j.value("type", "task");
    t.action = j.value("action", "");
    t.target = j.value("target", "");
    if (j.contains("parameters")) t.parameters = j["parameters"];
    t.requires_approval = j.value("requires_approval", false);
    t.created_at = j.value("created_at", (long long)0);
    t.execute_at = j.value("execute_at", (long long)0);
    t.repeat = j.value("repeat", false);
    t.interval_seconds = j.value("interval_seconds", 0);
    t.cron_expression = j.value("cron_expression", "");
    t.heartbeat_check_id = j.value("heartbeat_check_id", "");
    t.priority = j.value("priority", "normal");
    t.status = j.value("status", "pending");
    return t;
}

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start(TaskExecutor executor, int check_interval_ms) {
    if (running_) return;
    executor_ = std::move(executor);
    check_interval_ms_ = check_interval_ms;
    running_ = true;

    if (store_) {
        tasks_ = store_->load_all();
    }

    worker_ = std::thread(&Scheduler::loop, this);
    Logger::instance().info("Scheduler started with interval " +
        std::to_string(check_interval_ms_) + "ms");
}

void Scheduler::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    Logger::instance().info("Scheduler stopped");
}

bool Scheduler::add_task(const Task& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    Task t = task;
    if (t.id.empty()) t.id = generate_uuid();
    if (t.created_at == 0) {
        t.created_at = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    tasks_.push_back(t);
    if (store_) store_->add(t);
    Logger::instance().info("Task added: " + t.action + " [" + t.id + "]");
    return true;
}

bool Scheduler::cancel_task(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
        [&](const Task& t) { return t.id == id; });
    if (it == tasks_.end()) {
        Logger::instance().warning("Task not found for cancel: " + id);
        return false;
    }
    it->status = "cancelled";
    if (store_) store_->update(*it);
    tasks_.erase(it);
    Logger::instance().info("Task cancelled: " + id);
    return true;
}

std::vector<Task> Scheduler::list_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_;
}

std::vector<Task> Scheduler::list_tasks_by_status(const std::string& status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Task> result;
    for (auto& t : tasks_) {
        if (t.status == status) result.push_back(t);
    }
    return result;
}

void Scheduler::loop() {
    while (running_) {
        check_and_execute();
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms_));
    }
}

void Scheduler::check_and_execute() {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<Task> due_tasks;
    {
            std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(tasks_.begin(), tasks_.end(),
            [&](const Task& t) {
                if (t.status != "pending") return false;
                if (t.execute_at > 0 && t.execute_at <= now) return true;
                return false;
            });
        due_tasks.assign(it, tasks_.end());
        tasks_.erase(it, tasks_.end());
    }

    for (auto& task : due_tasks) {
        task.status = "running";
        Logger::instance().info("Executing task: " + task.action + " [" + task.id + "]");

        if (executor_) {
            try {
                executor_(task);
            } catch (const std::exception& e) {
                Logger::instance().err("Task execution failed [" +
                    task.id + "]: " + e.what());
                task.status = "failed";
            }
        }

        // Re-schedule if repeating
        if (task.repeat && task.interval_seconds > 0) {
            task.execute_at = now + task.interval_seconds;
            task.status = "pending";
            {
                std::lock_guard<std::mutex> lock(mutex_);
                tasks_.push_back(task);
            }
            if (store_) store_->add(task);
        } else {
            task.status = "completed";
            if (store_) store_->update(task);
        }
    }
}

}
