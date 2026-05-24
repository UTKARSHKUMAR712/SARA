#pragma once
#include <string>

namespace sara {

class AutomationDock {
public:
    static void send_dock(const std::string& chat_id);
    static void handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id);
};

}
