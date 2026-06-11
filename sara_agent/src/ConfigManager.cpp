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

        // Terminal runtime config
        cfg.terminal_host           = j.value("terminal_host", "localhost");
        cfg.terminal_port           = j.value("terminal_port", 9081);
        cfg.terminal_shell          = j.value("terminal_shell", "powershell.exe");
        cfg.terminal_expiry_minutes = j.value("terminal_expiry_minutes", 30);
        cfg.terminal_default_cols   = j.value("terminal_default_cols", 220);
        cfg.terminal_default_rows   = j.value("terminal_default_rows", 50);

        // Cloudflare Tunnel config
        cfg.cloudflare_enabled     = j.value("cloudflare_enabled", true);
        cfg.cloudflare_mode        = j.value("cloudflare_mode", std::string("quick"));
        cfg.cloudflare_tunnel_name = j.value("cloudflare_tunnel_name", std::string(""));
        cfg.cloudflare_exe_dir     = j.value("cloudflare_exe_dir", std::string(""));

        // File Browser config
        cfg.filebrowser_enabled    = j.value("filebrowser_enabled", true);
        cfg.filebrowser_port       = j.value("filebrowser_port", 9090);
        cfg.filebrowser_root       = j.value("filebrowser_root", std::string("C:\\\\"));

        cfg.proxy_header_timeout_seconds = j.value("proxy_header_timeout_seconds", 300);
        cfg.proxy_idle_timeout_seconds   = j.value("proxy_idle_timeout_seconds", 300);

        // MeshCentral config
        cfg.meshcentral.enabled = j.value("meshcentral_enabled", true);
        cfg.meshcentral.port = j.value("meshcentral_port", 4430);
        cfg.meshcentral.autostart = j.value("meshcentral_autostart", true);
        cfg.meshcentral.auto_install = j.value("meshcentral_auto_install", true);
        cfg.meshcentral.cloudflare_tunnel = j.value("meshcentral_cloudflare_tunnel", true);

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

    j["terminal_host"]           = config_.terminal_host;
    j["terminal_port"]           = config_.terminal_port;
    j["terminal_shell"]          = config_.terminal_shell;
    j["terminal_expiry_minutes"] = config_.terminal_expiry_minutes;
    j["terminal_default_cols"]   = config_.terminal_default_cols;
    j["terminal_default_rows"]   = config_.terminal_default_rows;

    j["filebrowser_enabled"]     = config_.filebrowser_enabled;
    j["filebrowser_port"]        = config_.filebrowser_port;
    j["filebrowser_root"]        = config_.filebrowser_root;
    
    j["proxy_header_timeout_seconds"] = config_.proxy_header_timeout_seconds;
    j["proxy_idle_timeout_seconds"]   = config_.proxy_idle_timeout_seconds;

    j["meshcentral_enabled"] = config_.meshcentral.enabled;
    j["meshcentral_port"] = config_.meshcentral.port;
    j["meshcentral_autostart"] = config_.meshcentral.autostart;
    j["meshcentral_auto_install"] = config_.meshcentral.auto_install;
    j["meshcentral_cloudflare_tunnel"] = config_.meshcentral.cloudflare_tunnel;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2);
    return true;
}

}
