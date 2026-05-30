#pragma once
#include <string>

namespace sara {

class SpotifyPlugin {
public:
    static SpotifyPlugin& instance();

    void start();
    void stop();

    // Returns true if the command was handled
    bool handle_command(const std::string& chat_id, const std::string& text);

    // Called from DockRouter for button callbacks
    bool handle_callback(const std::string& chat_id,
                         const std::string& callback_data,
                         const std::string& callback_query_id,
                         int message_id);
};

} // namespace sara
