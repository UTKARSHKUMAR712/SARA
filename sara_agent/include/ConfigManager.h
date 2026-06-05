#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

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
    std::unordered_map<std::string, PluginConfig> plugins;
    std::string log_level = "INFO";
    std::string log_dir = "logs";
    std::string data_dir = "data";
    int scheduler_interval_ms = 500;
    std::vector<std::string> trusted_admins;

    // ── Remote Terminal Runtime ────────────────────────────────────────────
    std::string terminal_host           = "localhost";    // IP/hostname in Telegram URLs
    int         terminal_port           = 9081;           // HTTP+WS port for terminal
    std::string terminal_shell          = "powershell.exe"; // default shell
    int         terminal_expiry_minutes = 30;             // session timeout
    int         terminal_default_cols   = 220;
    int         terminal_default_rows   = 50;

    // ── Cloudflare Tunnel ─────────────────────────────────────────────────
    bool        cloudflare_enabled      = true;           // auto-start cloudflared
    std::string cloudflare_mode         = "quick";        // "quick" or "named"
    std::string cloudflare_tunnel_name  = "";             // for named tunnel mode
    std::string cloudflare_exe_dir      = "";             // dir to store cloudflared.exe

    // ── File Browser ─────────────────────────────────────────────────────
    bool        filebrowser_enabled     = true;           // auto-start filebrowser.exe
    int         filebrowser_port        = 9090;           // localhost port for filebrowser
    std::string filebrowser_root        = "C:\\";         // root directory to expose
    int         proxy_header_timeout_seconds = 300;
    int         proxy_idle_timeout_seconds   = 300;
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
