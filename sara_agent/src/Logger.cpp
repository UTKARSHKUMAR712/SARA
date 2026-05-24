#include "../include/Logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace sara {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& log_dir, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    namespace fs = std::filesystem;
    fs::create_directories(log_dir);
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << log_dir << "\\sara_"
        << std::put_time(&tm, "%Y%m%d")
        << ".log";
    log_file_.open(oss.str(), std::ios::app);
    min_level_ = min_level;
    initialized_ = true;
}

void Logger::set_min_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        std::cerr << "[NOT INITIALIZED] " << message << std::endl;
        return;
    }
    if (level < min_level_) return;
    auto ts = current_timestamp();
    auto lvl = level_to_string(level);
    log_file_ << ts << " [" << lvl << "] " << message << std::endl;
    log_file_.flush();
}

void Logger::debug(const std::string& message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string& message) { log(LogLevel::INFO, message); }
void Logger::warning(const std::string& message) { log(LogLevel::WARNING, message); }
void Logger::err(const std::string& message) { log(LogLevel::ERR, message); }

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_.is_open()) {
        log_file_.close();
    }
    initialized_ = false;
}

Logger::~Logger() {
    shutdown();
}

std::string Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERR:   return "ERROR";
    }
    return "UNKNOWN";
}

std::string Logger::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm;
    localtime_s(&tm, &tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

}
