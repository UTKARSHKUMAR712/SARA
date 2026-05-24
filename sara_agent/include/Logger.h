#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>

namespace sara {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR
};

class Logger {
public:
    static Logger& instance();

    void init(const std::string& log_dir, LogLevel min_level = LogLevel::INFO);
    void set_min_level(LogLevel level);
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void err(const std::string& message);
    void shutdown();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string level_to_string(LogLevel level);
    std::string current_timestamp();

    std::ofstream log_file_;
    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::INFO;
    bool initialized_ = false;
};

}
