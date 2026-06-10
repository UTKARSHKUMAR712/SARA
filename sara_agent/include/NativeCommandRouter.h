#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

class NativeCommandRouter {
public:
    // Returns true if the command was natively handled, false otherwise.
    static bool handle(const std::string& chat_id, const std::string& text);
    // Helpers
    static void execute_and_reply(const std::string& chat_id, const std::string& action, const nlohmann::json& params = nlohmann::json::object(), bool send_reply = true);

private:
    static bool handle_volume(const std::string& chat_id, const std::string& text);
    static bool handle_brightness(const std::string& chat_id, const std::string& text);
    static bool handle_media(const std::string& chat_id, const std::string& text);
    static bool handle_system(const std::string& chat_id, const std::string& text);
    static bool handle_screenshot(const std::string& chat_id, const std::string& text);
    static bool handle_camera(const std::string& chat_id, const std::string& text);
    static bool handle_apps(const std::string& chat_id, const std::string& text);
    static bool handle_network(const std::string& chat_id, const std::string& text);
    static bool handle_clipboard(const std::string& chat_id, const std::string& text);
    static bool handle_monitoring(const std::string& chat_id, const std::string& text);
    static bool handle_window(const std::string& chat_id, const std::string& text);
    static bool handle_automation(const std::string& chat_id, const std::string& text);
    static bool handle_file(const std::string& chat_id, const std::string& text);
    static bool handle_hotkey(const std::string& chat_id, const std::string& text);
    static bool handle_search(const std::string& chat_id, const std::string& text);
    static bool handle_news(const std::string& chat_id, const std::string& text);
};

}
