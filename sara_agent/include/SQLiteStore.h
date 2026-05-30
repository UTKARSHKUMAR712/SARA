#pragma once
#include <string>
#include <vector>
#include <utility>
#include "../include/Scheduler.h"

// sqlite3 must be forward-declared OUTSIDE any namespace
struct sqlite3;

namespace sara {

struct EventRule; // forward declaration from EventAutomation.h

class SQLiteStore : public TaskStore {
public:
    SQLiteStore();
    ~SQLiteStore();

    bool open(const std::string& db_path);
    void close();

    // Task CRUD
    bool add(const Task& task) override;
    bool remove(const std::string& id) override;
    bool update(const Task& task) override;
    std::vector<Task> load_all() override;
    std::vector<Task> load_due(long long now) override;

    // Event rules CRUD
    bool                   event_rule_add(const EventRule& rule);
    bool                   event_rule_remove(const std::string& id);
    std::vector<EventRule> event_rule_list();

private:
    bool init_schema();
    bool execute(const std::string& sql);

    sqlite3* db_ = nullptr;  // global ::sqlite3, not sara::sqlite3
};

} // namespace sara
