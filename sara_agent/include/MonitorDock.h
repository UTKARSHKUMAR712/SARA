#pragma once
#include <string>

namespace sara {

class MonitorDock {
public:
    static void send_dock(const std::string& chat_id);
    static void handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id);

private:
    static std::string generate_text();
    static std::string generate_bar(double percent, int length = 10);
};

}
