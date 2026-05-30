#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "WinAPIExecutor.h"

namespace sara {

    ActionResult dispatch_action(const std::string& act, const nlohmann::json& params);
    void send_file_result(const std::string& chat_id, const ActionResult& res, const std::string& action_name);

}
