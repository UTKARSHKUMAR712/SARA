#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct ActionResult {
    bool        success = false;
    std::string message;
    json        data = json::object();
};

class WinAPIExecutor {
public:
    ActionResult execute(const std::string& action, const json& params);

    // Process control
    ActionResult open_app(const std::string& target, const json& params);
    ActionResult close_process(const std::string& target, const json& params);
    ActionResult enum_processes();
    ActionResult focus_window(const std::string& title);
    ActionResult enum_windows(const std::string& filter);

    // Input
    ActionResult send_keys(const std::string& keys);

    // Shell (internal)
    ActionResult run_cmd(const std::string& command, int timeout_sec);
    ActionResult run_powershell(const std::string& command, int timeout_sec);

    // Notifications
    ActionResult notify(const std::string& title, const std::string& message);

    // Audio
    ActionResult volume_set(int level);
    ActionResult volume_mute(bool mute);

    // Clipboard
    ActionResult clipboard_write(const std::string& text);
    ActionResult clipboard_read();

    // Network
    ActionResult get_ip_address();

    // Semantic browser/media tools
    ActionResult play_youtube(const std::string& query);
    ActionResult search_google(const std::string& query);
    ActionResult open_website(const std::string& url);

    // Display
    ActionResult change_brightness(int level);

    // System
    ActionResult lock_pc();
    ActionResult write_file(const std::string& path, const std::string& content);
    ActionResult read_file(const std::string& path);
    ActionResult list_dir(const std::string& path);
};

} // namespace sara

