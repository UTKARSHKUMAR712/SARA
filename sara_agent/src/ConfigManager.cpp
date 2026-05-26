#include "../include/ConfigManager.h"
#include <fstream>
#include <sstream>

namespace sara {

bool ConfigManager::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        last_error_ = "Cannot open config file: " + path;
        return false;
    }
    try {
        json j;
        file >> j;

        auto& cfg = config_;

        if (j.contains("telegram")) {
            auto& t = j["telegram"];
            cfg.telegram.token = t.value("token", "");
            cfg.telegram.password = t.value("password", "test123f");
            cfg.telegram.polling_interval_ms = t.value("polling_interval_ms", 25000);
            if (t.contains("allowed_user_ids")) {
                for (auto& id : t["allowed_user_ids"]) {
                    cfg.telegram.allowed_user_ids.push_back(id.get<long long>());
                }
            }
        }

        cfg.active_profile = j.value("active_profile", "default");

        if (j.contains("ai_profiles")) {
            for (auto& [key, val] : j["ai_profiles"].items()) {
                AIProviderConfig pc;
                pc.provider = val.value("provider", "");
                pc.endpoint = val.value("endpoint", "");
                pc.api_key = val.value("api_key", "");
                pc.model = val.value("model", "");
                pc.max_tokens = val.value("max_tokens", 4096);
                pc.temperature = val.value("temperature", 0.7);
                pc.timeout_seconds = val.value("timeout_seconds", 60);
                pc.thinking_enabled = val.value("thinking_enabled", false);
                cfg.ai_profiles[key] = pc;
            }
        }

        // Load ai_primary from explicit key, then fall back to active_profile
        if (j.contains("ai_primary") && !j["ai_primary"].value("api_key", "").empty()) {
            auto& a = j["ai_primary"];
            cfg.ai_primary.provider = a.value("provider", "");
            cfg.ai_primary.endpoint = a.value("endpoint", "");
            cfg.ai_primary.api_key = a.value("api_key", "");
            cfg.ai_primary.model = a.value("model", "");
            cfg.ai_primary.max_tokens = a.value("max_tokens", 4096);
            cfg.ai_primary.temperature = a.value("temperature", 0.7);
            cfg.ai_primary.timeout_seconds = a.value("timeout_seconds", 60);
            cfg.ai_primary.thinking_enabled = a.value("thinking_enabled", false);
        } else if (!cfg.active_profile.empty() && cfg.ai_profiles.count(cfg.active_profile)) {
            // Use active_profile as primary
            cfg.ai_primary = cfg.ai_profiles.at(cfg.active_profile);
        }

        if (j.contains("ai_fallback") && !j["ai_fallback"].value("api_key", "").empty()) {
            auto& a = j["ai_fallback"];
            cfg.ai_fallback.provider = a.value("provider", "");
            cfg.ai_fallback.endpoint = a.value("endpoint", "");
            cfg.ai_fallback.api_key = a.value("api_key", "");
            cfg.ai_fallback.model = a.value("model", "");
            cfg.ai_fallback.max_tokens = a.value("max_tokens", 2048);
            cfg.ai_fallback.temperature = a.value("temperature", 0.7);
            cfg.ai_fallback.timeout_seconds = a.value("timeout_seconds", 30);
        }

        cfg.log_level = j.value("log_level", "INFO");
        cfg.log_dir = j.value("log_dir", "logs");
        cfg.data_dir = j.value("data_dir", "data");
        cfg.scheduler_interval_ms = j.value("scheduler_interval_ms", 500);

        if (j.contains("trusted_admins")) {
            for (auto& admin : j["trusted_admins"]) {
                cfg.trusted_admins.push_back(admin.get<std::string>());
            }
        }

        if (j.contains("plugins")) {
            for (auto& [key, val] : j["plugins"].items()) {
                PluginConfig pc;
                pc.enabled = val.value("enabled", true);
                pc.entry = val.value("entry", "");
                cfg.plugins[key] = pc;
            }
        }

    } catch (const std::exception& e) {
        last_error_ = "Config parse error: ";
        last_error_ += e.what();
        return false;
    }
    return true;
}

bool ConfigManager::save(const std::string& path) const {
    json j;
    j["telegram"]["token"] = config_.telegram.token;
    j["telegram"]["password"] = config_.telegram.password;
    j["telegram"]["polling_interval_ms"] = config_.telegram.polling_interval_ms;
    for (auto& id : config_.telegram.allowed_user_ids) {
        j["telegram"]["allowed_user_ids"].push_back(id);
    }

    j["active_profile"] = config_.active_profile;
    for (auto& [key, val] : config_.ai_profiles) {
        j["ai_profiles"][key]["provider"] = val.provider;
        j["ai_profiles"][key]["endpoint"] = val.endpoint;
        j["ai_profiles"][key]["api_key"] = val.api_key;
        j["ai_profiles"][key]["model"] = val.model;
        j["ai_profiles"][key]["max_tokens"] = val.max_tokens;
        j["ai_profiles"][key]["temperature"] = val.temperature;
        j["ai_profiles"][key]["timeout_seconds"] = val.timeout_seconds;
        j["ai_profiles"][key]["thinking_enabled"] = val.thinking_enabled;
    }

    j["ai_primary"]["provider"] = config_.ai_primary.provider;
    j["ai_primary"]["endpoint"] = config_.ai_primary.endpoint;
    j["ai_primary"]["api_key"] = config_.ai_primary.api_key;
    j["ai_primary"]["model"] = config_.ai_primary.model;
    j["ai_primary"]["max_tokens"] = config_.ai_primary.max_tokens;
    j["ai_primary"]["temperature"] = config_.ai_primary.temperature;
    j["ai_primary"]["timeout_seconds"] = config_.ai_primary.timeout_seconds;
    j["ai_primary"]["thinking_enabled"] = config_.ai_primary.thinking_enabled;

    j["ai_fallback"]["provider"] = config_.ai_fallback.provider;
    j["ai_fallback"]["endpoint"] = config_.ai_fallback.endpoint;
    j["ai_fallback"]["api_key"] = config_.ai_fallback.api_key;
    j["ai_fallback"]["model"] = config_.ai_fallback.model;

    j["log_level"] = config_.log_level;
    j["log_dir"] = config_.log_dir;
    j["data_dir"] = config_.data_dir;
    j["scheduler_interval_ms"] = config_.scheduler_interval_ms;

    for (auto& admin : config_.trusted_admins) {
        j["trusted_admins"].push_back(admin);
    }

    for (auto& [name, pc] : config_.plugins) {
        j["plugins"][name]["enabled"] = pc.enabled;
        j["plugins"][name]["entry"] = pc.entry;
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

}
