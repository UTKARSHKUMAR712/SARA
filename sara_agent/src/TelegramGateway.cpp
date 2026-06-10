#include "../include/TelegramGateway.h"
#include "../include/Logger.h"
#include "../include/WebSocketServer.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <chrono>
#include <ctime>

#include <thread>
#pragma comment(lib, "winhttp.lib")

extern ::sara::WebSocketServer g_ws_server;
extern std::string resolve_path(const std::string& path);

namespace sara {

TelegramGateway::TelegramGateway() = default;
TelegramGateway::~TelegramGateway() { stop(); }

bool TelegramGateway::start(const std::string& token, long polling_interval_ms) {
    if (running_) return true;
    if (token.empty() || token == "YOUR_TELEGRAM_BOT_TOKEN") {
        Logger::instance().warning("Telegram token not configured");
        return false;
    }
    token_ = token;
    polling_interval_ms_ = polling_interval_ms;
    
    // Flush old updates before starting poll loop
    try {
        json payload = {{"offset", 0}, {"timeout", 0}, {"allowed_updates", json::array({"message", "callback_query"})}};
        auto result = api_call("getUpdates", payload);
        if (result.contains("result") && result["result"].is_array()) {
            for (auto& update : result["result"]) {
                if (update.contains("update_id")) {
                    long long uid = update["update_id"].get<long long>();
                    if (uid > last_update_id_) last_update_id_ = uid;
                }
            }
        }
    } catch (...) {}

    running_ = true;
    poll_thread_ = std::thread(&TelegramGateway::poll_loop, this);
    Logger::instance().info("Telegram gateway started");
    try {
        set_my_commands();
    } catch (const std::exception& e) {
        Logger::instance().err("Error setting Telegram commands: " + std::string(e.what()));
    } catch (...) {
        Logger::instance().err("Unknown error setting Telegram commands");
    }
    return true;
}

void TelegramGateway::stop() {
    running_ = false;
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    Logger::instance().info("Telegram gateway stopped");
}

bool TelegramGateway::set_my_commands() {
    try {
        json payload = {
            {"commands", json::array({
                {{"command", "dock"}, {"description", "Open Media Dock"}},
                {{"command", "system"}, {"description", "Open System Dock"}},
                {{"command", "status"}, {"description", "Runtime status"}},
                {{"command", "help"}, {"description", "Show all commands"}},
                {{"command", "terminal"}, {"description", "Start browser terminal"}},
                {{"command", "terminal_admin"}, {"description", "Start admin browser terminal"}},
                {{"command", "sararestart"}, {"description", "Restart SARA and Cloudflare completely"}},
                {{"command", "sarashutdown"}, {"description", "Kill SARA completely (no restart)"}},
                {{"command", "screenshot"}, {"description", "Take screenshot"}},
                {{"command", "photo"}, {"description", "Webcam photo"}},
                {{"command", "monitor"}, {"description", "Live system stats"}},
                {{"command", "tasks"}, {"description", "Scheduled tasks"}},
                {{"command", "rules"}, {"description", "Event automation rules"}},
                {{"command", "files"}, {"description", "Manage files via File Browser"}},
                {{"command", "filesshutdown"}, {"description", "Shutdown File Browser completely"}},
                {{"command", "workspace"}, {"description", "Open SARA IDE workspace"}},
                {{"command", "workspaceshutdown"}, {"description", "Shutdown SARA IDE workspace"}},
                {{"command", "sp_help"}, {"description", "Spotify commands list"}}
            })}
        };
        auto result = api_call("setMyCommands", payload);
        return result.value("ok", false);
    } catch (const std::exception& e) {
        Logger::instance().err("Exception in set_my_commands: " + std::string(e.what()));
        return false;
    } catch (...) {
        Logger::instance().err("Unknown exception in set_my_commands");
        return false;
    }
}

std::string TelegramGateway::build_url(const std::string& method) const {
    return "https://api.telegram.org/bot" + token_ + "/" + method;
}

void TelegramGateway::poll_loop() {
    while (running_) {
        try {
            json payload = {{"offset", last_update_id_ + 1},
                            {"timeout", static_cast<int>(polling_interval_ms_ / 1000)},
                            {"allowed_updates", json::array({"message", "callback_query"})}};
            auto result = api_call("getUpdates", payload);

            if (result.contains("result") && result["result"].is_array()) {
                for (auto& update : result["result"]) {
                    handle_update(update);
                }
            }
        } catch (const std::exception& e) {
            Logger::instance().err("Telegram poll error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep on error to prevent spam
        }
    }
}

void TelegramGateway::handle_update(const json& update) {
    if (update.contains("update_id")) {
        last_update_id_ = update["update_id"].get<long long>();
    }
    
    json msg;
    std::string text = "";
    std::string chat_id = "";
    std::string from_name = "";
    long long user_id = 0;
    std::string callback_data = "";
    std::string callback_query_id = "";
    int message_id = 0;

    if (update.contains("callback_query")) {
        auto& cb = update["callback_query"];
        if (cb.contains("message")) {
            msg = cb["message"];
            chat_id = std::to_string(msg["chat"]["id"].get<long long>());
            message_id = msg.value("message_id", 0);
        } else {
            return;
        }
        callback_query_id = cb.value("id", "");
        callback_data = cb.value("data", "");
        from_name = cb["from"].value("first_name", "");
        user_id = cb["from"].value("id", 0LL);
        text = "[CALLBACK] " + callback_data; // Prepend to easily distinguish in logs/handler
    } 
    else if (update.contains("message")) {
        msg = update["message"];
        if (!msg.contains("text") || !msg.contains("chat")) return;
        text = msg["text"].get<std::string>();
        chat_id = std::to_string(msg["chat"]["id"].get<long long>());
        from_name = msg["from"].value("first_name", "");
        user_id = msg["from"].value("id", 0LL);
        message_id = msg.value("message_id", 0);
    } 
    else {
        return;
    }

    if (callback_data.empty()) {
        Logger::instance().info("Telegram msg from " + chat_id + ": " + text);
    } else {
        Logger::instance().info("Telegram callback from " + chat_id + ": " + callback_data);
    }

    {
        std::lock_guard<std::mutex> lock(msg_mutex_);
        TelegramMessage tm;
        tm.update_id = last_update_id_;
        tm.chat_id = chat_id;
        tm.text = text;
        tm.from_name = from_name;
        tm.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        tm.message_id = message_id;
        tm.callback_data = callback_data;
        tm.callback_query_id = callback_query_id;
        recent_messages_.push_back(std::move(tm));
        if (recent_messages_.size() > 100) recent_messages_.pop_front();
    }

    if (handler_) {
        // Build a synthetic message object for the handler to process
        json raw_msg = msg;
        raw_msg["sara_user_id"] = user_id;
        if (!callback_data.empty()) {
            raw_msg["callback_data"] = callback_data;
            raw_msg["callback_query_id"] = callback_query_id;
        }
        
        // Execute the handler asynchronously in a detached thread
        // to prevent blocking the Telegram Gateway poll loop.
        std::thread([this, chat_id, text, raw_msg]() {
            try {
                handler_(chat_id, text, raw_msg);
            } catch (const std::exception& e) {
                Logger::instance().err("Exception in async message handler: " + std::string(e.what()));
            } catch (...) {
                Logger::instance().err("Unknown exception in async message handler");
            }
        }).detach();
    }
}

// ── Progress / Live Streaming ──────────────────────────────────────────────
int TelegramGateway::send_progress(const std::string& chat_id,
                                    const std::string& text,
                                    int progress_pct) {
    std::string msg = text;
    if (progress_pct >= 0) {
        int filled = progress_pct / 10;
        msg += "\n`[";
        for (int i = 0; i < 10; i++) msg += (i < filled) ? "█" : "░";
        msg += "] " + std::to_string(progress_pct) + "%`";
    }
    return send_message(chat_id, msg);
}

bool TelegramGateway::edit_progress_message(const std::string& chat_id,
                                              int message_id,
                                              const std::string& text,
                                              int progress_pct) {
    std::string msg = text;
    if (progress_pct >= 0) {
        int filled = progress_pct / 10;
        msg += "\n`[";
        for (int i = 0; i < 10; i++) msg += (i < filled) ? "█" : "░";
        msg += "] " + std::to_string(progress_pct) + "%`";
    }
    return edit_message_text(chat_id, message_id, msg);
}

bool TelegramGateway::send_chunked_log(const std::string& chat_id,
                                        const std::string& log_text,
                                        const std::string& header) {
    // Telegram max message length is 4096 chars
    const size_t MAX_MSG = 3900;
    std::string remaining = log_text;

    if (!header.empty()) {
        std::string first = header + "\n```\n";
        size_t chunk = MAX_MSG - first.size() - 4;
        if (chunk > remaining.size()) chunk = remaining.size();
        first += remaining.substr(0, chunk) + "\n```";
        send_message(chat_id, first);
        remaining = remaining.substr(chunk);
    }

    while (!remaining.empty()) {
        size_t chunk = MAX_MSG - 8; // ```\n ... \n```
        if (chunk > remaining.size()) chunk = remaining.size();
        std::string part = "```\n" + remaining.substr(0, chunk) + "\n```";
        send_message(chat_id, part);
        remaining = remaining.substr(chunk);
    }
    return true;
}

json TelegramGateway::get_recent_messages(int count) {
    std::lock_guard<std::mutex> lock(msg_mutex_);
    json arr = json::array();
    auto start = recent_messages_.size() > (size_t)count
        ? recent_messages_.end() - count : recent_messages_.begin();
    for (auto it = start; it != recent_messages_.end(); ++it) {
        json entry;
        entry["chat_id"] = it->chat_id;
        entry["text"] = it->text;
        entry["from_name"] = it->from_name;
        entry["timestamp"] = it->timestamp;
        arr.push_back(std::move(entry));
    }
    return arr;
}



int TelegramGateway::send_message(const std::string& chat_id, const std::string& text, const std::string& parse_mode) {
    if (chat_id == "LAN") {
        ::g_ws_server.broadcast({{"type", "chat_reply"}, {"text", text}});
        return 1;
    }
    json payload = {{"chat_id", chat_id}, {"text", text}};
    if (!parse_mode.empty()) {
        payload["parse_mode"] = parse_mode;
    }
    auto result = api_call("sendMessage", payload);
    bool ok = result.value("ok", false);
    if (!ok) {
        Logger::instance().err("Telegram send failed: " + result.dump());
        return 0;
    }
    if (result.contains("result") && result["result"].contains("message_id")) {
        return result["result"]["message_id"].get<int>();
    }
    return 1;
}

bool TelegramGateway::send_inline_keyboard(const std::string& chat_id, const std::string& text, const json& inline_keyboard) {
    if (chat_id == "LAN") {
        ::g_ws_server.broadcast({{"type", "chat_reply"}, {"text", text}, {"keyboard", inline_keyboard}});
        return true;
    }
    json payload = {
        {"chat_id", chat_id},
        {"text", text},
        {"reply_markup", {{"inline_keyboard", inline_keyboard}}}
    };
    auto result = api_call("sendMessage", payload);
    bool ok = result.value("ok", false);
    if (!ok) {
        Logger::instance().err("Telegram send_inline_keyboard failed: " + result.dump());
    }
    return ok;
}

int TelegramGateway::send_with_keyboard(const std::string& chat_id, const std::string& text, const json& inline_keyboard) {
    if (chat_id == "LAN") {
        static int lan_msg_id_counter = 1000;
        int msg_id = ++lan_msg_id_counter;
        ::g_ws_server.broadcast({{"type", "chat_reply"}, {"text", text}, {"keyboard", inline_keyboard}, {"message_id", msg_id}});
        return msg_id;
    }
    json payload = {
        {"chat_id", chat_id},
        {"text", text},
        {"reply_markup", {{"inline_keyboard", inline_keyboard}}}
    };
    auto result = api_call("sendMessage", payload);
    bool ok = result.value("ok", false);
    if (!ok) {
        Logger::instance().err("Telegram send_with_keyboard failed: " + result.dump());
        return 0;
    }
    if (result.contains("result") && result["result"].contains("message_id")) {
        return result["result"]["message_id"].get<int>();
    }
    return 0;
}


bool TelegramGateway::edit_message_text(const std::string& chat_id, int message_id, const std::string& text, const json& inline_keyboard) {
    if (chat_id == "LAN") {
        json payload = {{"type", "chat_edit"}, {"message_id", message_id}, {"text", text}};
        if (!inline_keyboard.is_null()) {
            payload["keyboard"] = inline_keyboard;
        }
        ::g_ws_server.broadcast(payload);
        return true;
    }
    json payload = {
        {"chat_id", chat_id},
        {"message_id", message_id},
        {"text", text}
    };
    if (!inline_keyboard.is_null()) {
        payload["reply_markup"] = {{"inline_keyboard", inline_keyboard}};
    }
    auto result = api_call("editMessageText", payload);
    bool ok = result.value("ok", false);
    if (!ok) {
        Logger::instance().err("Telegram edit_message_text failed: " + result.dump());
    }
    return ok;
}

bool TelegramGateway::answer_callback_query(const std::string& callback_query_id, const std::string& text, bool show_alert) {
    if (callback_query_id.rfind("lan_cb_", 0) == 0) {
        if (!text.empty()) {
            ::g_ws_server.broadcast({{"type", "toast"}, {"text", text}});
        }
        return true;
    }
    json payload = {
        {"callback_query_id", callback_query_id}
    };
    if (!text.empty()) {
        payload["text"] = text;
        payload["show_alert"] = show_alert;
    }
    auto result = api_call("answerCallbackQuery", payload);
    bool ok = result.value("ok", false);
    if (!ok) {
        Logger::instance().err("Telegram answer_callback_query failed: " + result.dump());
    }
    return ok;
}

bool TelegramGateway::send_photo(const std::string& chat_id,
    const std::string& file_path, const std::string& caption)
{
    if (chat_id == "LAN") {
        std::string filename = file_path;
        auto pos = filename.find_last_of("\\/");
        if (pos != std::string::npos) filename = filename.substr(pos + 1);
        std::string dest_dir = ::resolve_path("frontend\\lan_dashboard\\images");
        CreateDirectoryA(dest_dir.c_str(), nullptr);
        std::string dest_path = dest_dir + "\\" + filename;
        CopyFileA(file_path.c_str(), dest_path.c_str(), FALSE);
        ::g_ws_server.broadcast({{"type", "chat_reply"}, {"text", caption}, {"photo", "/images/" + filename}});
        return true;
    }

    HANDLE hfile = CreateFileA(file_path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hfile == INVALID_HANDLE_VALUE) {
        Logger::instance().err("send_photo: cannot open " + file_path);
        return false;
    }

    DWORD fsize = GetFileSize(hfile, nullptr);
    std::vector<char> file_data(fsize);
    DWORD read;
    ReadFile(hfile, file_data.data(), fsize, &read, nullptr);
    CloseHandle(hfile);

    std::string boundary = "----SARA" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::string crlf = "\r\n";

    std::vector<char> body;
    auto append = [&](const std::string& s) {
        body.insert(body.end(), s.begin(), s.end());
    };

    append("--" + boundary + crlf);
    append("Content-Disposition: form-data; name=\"chat_id\"" + crlf + crlf);
    append(chat_id + crlf);

    std::string fname = file_path;
    auto pos = fname.find_last_of("\\/");
    if (pos != std::string::npos) fname = fname.substr(pos + 1);

    append("--" + boundary + crlf);
    append("Content-Disposition: form-data; name=\"photo\"; filename=\"" + fname + "\"" + crlf);
    append("Content-Type: image/bmp" + crlf + crlf);
    body.insert(body.end(), file_data.begin(), file_data.end());
    append(crlf);

    if (!caption.empty()) {
        append("--" + boundary + crlf);
        append("Content-Disposition: form-data; name=\"caption\"" + crlf + crlf);
        append(caption + crlf);
    }

    append("--" + boundary + "--" + crlf);

    auto url = build_url("sendPhoto");
    auto host_start = url.find("://") + 3;
    auto path_start = url.find('/', host_start);
    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);

    HINTERNET session = WinHttpOpen(L"SARA/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return false;

    // Prevent blocking indefinitely on network calls
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);

    std::wstring whost(host.begin(), host.end());
    HINTERNET connect = WinHttpConnect(session, whost.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", wpath.c_str(),
        nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false; }

    std::wstring wboundary(boundary.begin(), boundary.end());
    std::wstring content_type = L"Content-Type: multipart/form-data; boundary=" + wboundary + L"\r\n";
    if (!WinHttpSendRequest(request, content_type.c_str(), content_type.size(),
        body.data(), body.size(), body.size(), 0))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        Logger::instance().err("send_photo: WinHttpSendRequest failed");
        return false;
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    std::string response;
    DWORD bytes_avail;
    while (WinHttpQueryDataAvailable(request, &bytes_avail) && bytes_avail > 0) {
        std::vector<char> buf(bytes_avail + 1);
        DWORD bytes_read;
        if (WinHttpReadData(request, buf.data(), bytes_avail, &bytes_read)) {
            buf[bytes_read] = 0;
            response += buf.data();
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    bool ok = false;
    try { ok = json::parse(response).value("ok", false); } catch (...) {}
    if (!ok) {
        Logger::instance().err("send_photo failed: " + response.substr(0, 500));
    }
    return ok;
}

bool TelegramGateway::send_action(const std::string& chat_id, const std::string& action) {
    json payload = {{"chat_id", chat_id}, {"action", action}};
    auto result = api_call("sendChatAction", payload);
    return result.value("ok", false);
}

bool TelegramGateway::send_document(const std::string& chat_id,
    const std::string& file_path, const std::string& caption)
{
    HANDLE hfile = CreateFileA(file_path.c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hfile == INVALID_HANDLE_VALUE) {
        Logger::instance().err("send_document: cannot open " + file_path);
        return false;
    }
    DWORD fsize = GetFileSize(hfile, nullptr);
    std::vector<char> file_data(fsize);
    DWORD read;
    ReadFile(hfile, file_data.data(), fsize, &read, nullptr);
    CloseHandle(hfile);

    std::string boundary = "----SARA" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count());
    std::string crlf = "\r\n";
    std::vector<char> body;
    auto append = [&](const std::string& s) {
        body.insert(body.end(), s.begin(), s.end());
    };

    std::string fname = file_path;
    auto pos = fname.find_last_of("\\/");
    if (pos != std::string::npos) fname = fname.substr(pos + 1);

    append("--" + boundary + crlf);
    append("Content-Disposition: form-data; name=\"chat_id\"" + crlf + crlf);
    append(chat_id + crlf);

    append("--" + boundary + crlf);
    append("Content-Disposition: form-data; name=\"document\"; filename=\"" + fname + "\"" + crlf);
    append("Content-Type: application/octet-stream" + crlf + crlf);
    body.insert(body.end(), file_data.begin(), file_data.end());
    append(crlf);

    if (!caption.empty()) {
        append("--" + boundary + crlf);
        append("Content-Disposition: form-data; name=\"caption\"" + crlf + crlf);
        append(caption + crlf);
    }
    append("--" + boundary + "--" + crlf);

    auto url         = build_url("sendDocument");
    auto host_start  = url.find("://") + 3;
    auto path_start  = url.find('/', host_start);
    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);

    HINTERNET session = WinHttpOpen(L"SARA/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return false;

    // Prevent blocking indefinitely on network calls
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);
    std::wstring whost(host.begin(), host.end());
    HINTERNET connect = WinHttpConnect(session, whost.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }
    std::wstring wpath(path.begin(), path.end());
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", wpath.c_str(),
        nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    std::wstring wboundary(boundary.begin(), boundary.end());
    std::wstring content_type =
        L"Content-Type: multipart/form-data; boundary=" + wboundary + L"\r\n";
    if (!WinHttpSendRequest(request, content_type.c_str(), content_type.size(),
        body.data(), body.size(), body.size(), 0))
    {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    WinHttpReceiveResponse(request, nullptr);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    Logger::instance().info("Document sent: " + fname);
    return true;
}

json TelegramGateway::api_call(const std::string& method, const json& payload) {
    auto url = build_url(method);
    auto host_start = url.find("://") + 3;
    auto path_start = url.find('/', host_start);
    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);

    HINTERNET session = WinHttpOpen(L"SARA/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!session) return {{"ok", false}};

    // Prevent blocking indefinitely on network calls
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 15000);

    std::wstring whost(host.begin(), host.end());
    HINTERNET connect = WinHttpConnect(session, whost.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return {{"ok", false}}; }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", wpath.c_str(),
        nullptr, nullptr, nullptr,
        WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return {{"ok", false}}; }

    std::string body = payload.dump();
    LPCSTR accept_types[] = {"application/json", nullptr};
    WinHttpSetOption(request, WINHTTP_OPTION_USER_AGENT, (LPVOID)L"SARA/1.0", sizeof(L"SARA/1.0"));

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(request, headers.c_str(), headers.size(),
        (LPVOID)body.c_str(), body.size(), body.size(), 0)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {{"ok", false}};
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {{"ok", false}};
    }

    std::string response;
    DWORD bytes_avail;
    while (WinHttpQueryDataAvailable(request, &bytes_avail) && bytes_avail > 0) {
        std::vector<char> buf(bytes_avail + 1);
        DWORD bytes_read;
        if (WinHttpReadData(request, buf.data(), bytes_avail, &bytes_read)) {
            buf[bytes_read] = 0;
            response += buf.data();
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    try {
        if (response.empty()) return {{"ok", false}};
        return json::parse(response);
    } catch (const std::exception& e) {
        Logger::instance().err("Telegram API parse error: " + std::string(e.what()) + ", response was: " + response);
        return {{"ok", false}};
    }
}

}
