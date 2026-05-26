#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;
using MessageHandler = std::function<void(const std::string& chat_id,
    const std::string& text, const json& raw_message)>;

struct TelegramMessage {
    long long update_id;
    std::string chat_id;
    std::string text;
    std::string from_name;
    long long timestamp;
    int message_id = 0;
    std::string callback_data;
    std::string callback_query_id;
};

class TelegramGateway {
public:
    TelegramGateway();
    ~TelegramGateway();

    bool start(const std::string& token, long polling_interval_ms = 2000);
    void stop();
    bool is_running() const { return running_; }
    
    bool set_my_commands();

    void set_message_handler(MessageHandler handler) { handler_ = std::move(handler); }

    int send_message(const std::string& chat_id, const std::string& text, const std::string& parse_mode = "");
    bool send_inline_keyboard(const std::string& chat_id, const std::string& text, const json& inline_keyboard);
    bool edit_message_text(const std::string& chat_id, int message_id, const std::string& text, const json& inline_keyboard = nullptr);
    bool answer_callback_query(const std::string& callback_query_id, const std::string& text = "", bool show_alert = false);
    
    bool send_photo(const std::string& chat_id, const std::string& file_path,
        const std::string& caption = "");
    bool send_document(const std::string& chat_id, const std::string& file_path,
        const std::string& caption = "");
    bool send_action(const std::string& chat_id, const std::string& action);

    json get_recent_messages(int count = 20);

    json api_call(const std::string& method, const json& payload);
private:
    void poll_loop();
    std::string build_url(const std::string& method) const;
    void handle_update(const json& update);

    std::string token_;
    std::atomic<bool> running_{false};
    std::thread poll_thread_;
    MessageHandler handler_;
    long long last_update_id_ = 0;
    long polling_interval_ms_ = 2000;

    std::deque<TelegramMessage> recent_messages_;
    std::mutex msg_mutex_;
};

}
