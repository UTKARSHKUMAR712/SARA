#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {
    void handle_telegram_message(const std::string& chat_id, std::string text, const nlohmann::json& raw_message);
    nlohmann::json handle_ipc_message(const nlohmann::json& msg);
    long long parse_delay_suffix(const std::string& original_text, std::string& clean_text);
}
