#include "../include/SQLiteStore.h"
#include "../include/EventAutomation.h"
#include "../include/Logger.h"
#include "../../shared/sqlite3.h"
#include <sstream>

namespace sara {

SQLiteStore::SQLiteStore() = default;

SQLiteStore::~SQLiteStore() {
    close();
}

bool SQLiteStore::open(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        Logger::instance().err("Failed to open SQLite DB: " +
            std::string(sqlite3_errmsg(db_)));
        return false;
    }
    if (!init_schema()) {
        return false;
    }
    Logger::instance().info("SQLite store opened: " + db_path);
    return true;
}

void SQLiteStore::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteStore::init_schema() {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id TEXT PRIMARY KEY,
            source TEXT DEFAULT '',
            type TEXT DEFAULT 'task',
            action TEXT NOT NULL,
            target TEXT DEFAULT '',
            parameters TEXT DEFAULT '{}',
            requires_approval INTEGER DEFAULT 0,
            created_at INTEGER DEFAULT 0,
            execute_at INTEGER DEFAULT 0,
            repeat INTEGER DEFAULT 0,
            interval_seconds INTEGER DEFAULT 0,
            priority TEXT DEFAULT 'normal',
            status TEXT DEFAULT 'pending'
        );
        CREATE TABLE IF NOT EXISTS permissions (
            plugin TEXT NOT NULL,
            permission TEXT NOT NULL,
            allowed INTEGER DEFAULT 0,
            PRIMARY KEY (plugin, permission)
        );
        CREATE TABLE IF NOT EXISTS sessions (
            session_id TEXT PRIMARY KEY,
            platform TEXT NOT NULL,
            chat_id TEXT DEFAULT '',
            context TEXT DEFAULT '{}',
            created_at INTEGER DEFAULT 0,
            last_activity INTEGER DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS memory (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS event_rules (
            id TEXT PRIMARY KEY,
            trigger_type TEXT NOT NULL,
            trigger_value TEXT NOT NULL DEFAULT '',
            actions TEXT NOT NULL DEFAULT '[]',
            enabled INTEGER DEFAULT 1,
            description TEXT DEFAULT '',
            created_at INTEGER DEFAULT 0
        );
    )";
    return execute(sql);
}


bool SQLiteStore::execute(const std::string& sql) {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        Logger::instance().err("SQLite error: " + std::string(err_msg));
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool SQLiteStore::add(const Task& task) {
    auto j = task.to_json();
    std::string params_str = j.value("parameters", json::object()).dump();

    const char* sql = R"(
        INSERT OR REPLACE INTO tasks
        (id, source, type, action, target, parameters, requires_approval,
         created_at, execute_at, repeat, interval_seconds, priority, status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        Logger::instance().err("SQLite prepare failed: " +
            std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, task.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, task.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, task.target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, params_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, task.requires_approval ? 1 : 0);
    sqlite3_bind_int64(stmt, 8, task.created_at);
    sqlite3_bind_int64(stmt, 9, task.execute_at);
    sqlite3_bind_int(stmt, 10, task.repeat ? 1 : 0);
    sqlite3_bind_int(stmt, 11, task.interval_seconds);
    sqlite3_bind_text(stmt, 12, task.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, task.status.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    if (!ok) {
        Logger::instance().err("SQLite insert failed: " +
            std::string(sqlite3_errmsg(db_)));
    }
    return ok;
}

bool SQLiteStore::remove(const std::string& id) {
    const char* sql = "DELETE FROM tasks WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool SQLiteStore::update(const Task& task) {
    const char* sql = R"(
        UPDATE tasks SET source=?, type=?, action=?, target=?, parameters=?,
        requires_approval=?, execute_at=?, repeat=?, interval_seconds=?,
        priority=?, status=? WHERE id=?;
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    auto j = task.to_json();
    std::string params_str = j.value("parameters", json::object()).dump();

    sqlite3_bind_text(stmt, 1, task.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, task.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, task.action.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, task.target.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, params_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, task.requires_approval ? 1 : 0);
    sqlite3_bind_int64(stmt, 7, task.execute_at);
    sqlite3_bind_int(stmt, 8, task.repeat ? 1 : 0);
    sqlite3_bind_int(stmt, 9, task.interval_seconds);
    sqlite3_bind_text(stmt, 10, task.priority.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, task.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, task.id.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Task> SQLiteStore::load_all() {
    std::vector<Task> tasks;
    const char* sql = "SELECT * FROM tasks WHERE status != 'completed' AND status != 'cancelled';";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tasks;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Task t;
        t.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        t.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        t.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        t.action = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        t.target = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        auto params_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (params_str) t.parameters = json::parse(params_str);
        t.requires_approval = sqlite3_column_int(stmt, 6) != 0;
        t.created_at = sqlite3_column_int64(stmt, 7);
        t.execute_at = sqlite3_column_int64(stmt, 8);
        t.repeat = sqlite3_column_int(stmt, 9) != 0;
        t.interval_seconds = sqlite3_column_int(stmt, 10);
        t.priority = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        t.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        tasks.push_back(t);
    }
    sqlite3_finalize(stmt);
    return tasks;
}

bool SQLiteStore::memory_set(const std::string& key, const std::string& value) {
    const char* sql = "INSERT OR REPLACE INTO memory (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::string SQLiteStore::memory_get(const std::string& key) {
    const char* sql = "SELECT value FROM memory WHERE key = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::string val;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto txt = sqlite3_column_text(stmt, 0);
        if (txt) val = reinterpret_cast<const char*>(txt);
    }
    sqlite3_finalize(stmt);
    return val;
}

std::vector<std::pair<std::string, std::string>> SQLiteStore::memory_get_all() {
    std::vector<std::pair<std::string, std::string>> entries;
    const char* sql = "SELECT key, value FROM memory;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return entries;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        auto v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (k && v) entries.emplace_back(k, v);
    }
    sqlite3_finalize(stmt);
    return entries;
}

std::vector<Task> SQLiteStore::load_due(long long now) {
    std::vector<Task> tasks;
    const char* sql = "SELECT * FROM tasks WHERE status='pending' AND execute_at > 0 AND execute_at <= ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return tasks;
    }
    sqlite3_bind_int64(stmt, 1, now);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Task t;
        t.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        t.source = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        t.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        t.action = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        t.target = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        auto params_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (params_str) t.parameters = json::parse(params_str);
        t.requires_approval = sqlite3_column_int(stmt, 6) != 0;
        t.created_at = sqlite3_column_int64(stmt, 7);
        t.execute_at = sqlite3_column_int64(stmt, 8);
        t.repeat = sqlite3_column_int(stmt, 9) != 0;
        t.interval_seconds = sqlite3_column_int(stmt, 10);
        t.priority = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        t.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        tasks.push_back(t);
    }
    sqlite3_finalize(stmt);
    return tasks;
}

// ─────────────────────────────────────────────────────────────────────────────
// Event Rules CRUD (still inside namespace sara)
// ─────────────────────────────────────────────────────────────────────────────

bool SQLiteStore::event_rule_add(const EventRule& rule) {
    const char* sql = R"(
        INSERT OR REPLACE INTO event_rules
        (id, trigger_type, trigger_value, actions, enabled, description, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    std::string acts = rule.actions.is_null() ? "[]" : rule.actions.dump();
    sqlite3_bind_text(stmt, 1, rule.id.c_str(),            -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rule.trigger_type.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rule.trigger_value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, acts.c_str(),               -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, rule.enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 6, rule.description.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, rule.created_at);

    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool SQLiteStore::event_rule_remove(const std::string& id) {
    const char* sql = "DELETE FROM event_rules WHERE id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<EventRule> SQLiteStore::event_rule_list() {
    std::vector<EventRule> rules;
    const char* sql = "SELECT id, trigger_type, trigger_value, actions, enabled, description, created_at FROM event_rules;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return rules;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EventRule r;
        auto col = [&](int c) -> const char* {
            auto t = sqlite3_column_text(stmt, c);
            return t ? reinterpret_cast<const char*>(t) : "";
        };
        r.id            = col(0);
        r.trigger_type  = col(1);
        r.trigger_value = col(2);
        try { r.actions = json::parse(col(3)); } catch (...) { r.actions = json::array(); }
        r.enabled       = sqlite3_column_int(stmt, 4) != 0;
        r.description   = col(5);
        r.created_at    = sqlite3_column_int64(stmt, 6);
        rules.push_back(r);
    }
    sqlite3_finalize(stmt);
    return rules;
}

} // namespace sara

