#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

class DockRouter {
public:
    static bool handle_command(const std::string& chat_id, const std::string& command);
    static bool handle_callback(const std::string& chat_id, const std::string& callback_data, const std::string& callback_query_id, int message_id);
};

}
