#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct AIProviderConfig {
    std::string provider;
    std::string endpoint;
    std::string api_key;
    std::string model;
    int max_tokens = 1024;
    double temperature = 0.7;
    int timeout_seconds = 30;
};

struct TelegramConfig {
    std::string token;
    std::string password = "test123f";
    long polling_interval_ms = 25000;
    std::vector<long long> allowed_user_ids;
};

struct PluginConfig {
    bool enabled = true;
    std::string entry;
};

struct AppConfig {
    TelegramConfig telegram;
    AIProviderConfig ai_primary;
    AIProviderConfig ai_fallback;
    std::unordered_map<std::string, PluginConfig> plugins;
    std::string log_level = "INFO";
    std::string log_dir = "logs";
    std::string data_dir = "data";
    int scheduler_interval_ms = 500;
    std::vector<std::string> trusted_admins;
};

class ConfigManager {
public:
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    const AppConfig& get() const { return config_; }
    AppConfig& get() { return config_; }
    const std::string& last_error() const { return last_error_; }

private:
    AppConfig config_;
    std::string last_error_;
};

}
