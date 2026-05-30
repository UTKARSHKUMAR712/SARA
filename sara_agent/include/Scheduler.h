#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>
#include <mutex>

namespace sara {

using json = nlohmann::json;

struct Task {
    std::string id;
    std::string source;
    std::string source_id;
    std::string type = "task";
    std::string action;
    std::string target;
    json parameters;
    bool requires_approval = false;
    long long created_at = 0;
    long long execute_at = 0;
    bool repeat = false;
    int interval_seconds = 0;
    std::string cron_expression; // cron-style: "*/5 * * * *" or "@every_30s"
    std::string priority = "normal";
    std::string status = "pending";
    std::string heartbeat_check_id;

    json to_json() const;
    static Task from_json(const json& j);
};

using TaskExecutor = std::function<void(const Task&)>;

class TaskStore {
public:
    virtual ~TaskStore() = default;
    virtual bool add(const Task& task) = 0;
    virtual bool remove(const std::string& id) = 0;
    virtual bool update(const Task& task) = 0;
    virtual std::vector<Task> load_all() = 0;
    virtual std::vector<Task> load_due(long long now) = 0;
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void start(TaskExecutor executor, int check_interval_ms = 1000);
    void stop();
    bool is_running() const { return running_; }

    void set_store(TaskStore* store) { store_ = store; }
    bool add_task(const Task& task);
    bool cancel_task(const std::string& id);
    std::vector<Task> list_tasks() const;
    std::vector<Task> list_tasks_by_status(const std::string& status) const;

private:
    void loop();
    void check_and_execute();

    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    TaskExecutor executor_;
    TaskStore* store_ = nullptr;
    std::vector<Task> tasks_;
    int check_interval_ms_ = 1000;
};

}
