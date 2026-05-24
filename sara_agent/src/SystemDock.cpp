#include "../include/SystemDock.h"
#include "../include/TelegramGateway.h"
#include "../include/WinAPIExecutor.h"
#include "../include/ScreenshotCapture.h"
#include "../include/CameraCapture.h"
#include "../include/Logger.h"
#include <windows.h>

extern sara::TelegramGateway g_telegram;
extern sara::WinAPIExecutor g_executor;

namespace sara {

void SystemDock::send_dock(const std::string& chat_id) {
    json inline_keyboard = json::array({
        json::array({
            {{"text", "📸 Screenshot"}, {"callback_data", "dock_sys:screenshot"}},
            {{"text", "📷 Camera"}, {"callback_data", "dock_sys:camera"}}
        }),
        json::array({
            {{"text", "🌙 Bright -"}, {"callback_data", "dock_sys:bright_down"}},
            {{"text", "☀️ Bright +"}, {"callback_data", "dock_sys:bright_up"}}
        }),
        json::array({
            {{"text", "🔒 Lock PC"}, {"callback_data", "dock_sys:lock_confirm"}},
            {{"text", "🔄 Restart"}, {"callback_data", "dock_sys:restart_confirm"}},
            {{"text", "⚡ Shutdown"}, {"callback_data", "dock_sys:shutdown_confirm"}}
        })
    });

    g_telegram.send_inline_keyboard(chat_id, "💻 **SYSTEM DOCK**", inline_keyboard);
}

void SystemDock::handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id, int message_id) {
    Logger::instance().info("SystemDock action: " + action);

    if (action == "screenshot") {
        g_telegram.answer_callback_query(callback_query_id, "Taking screenshot...");
        ScreenshotCapture cap;
        auto res = cap.capture_fullscreen("media");
        if (res.success && !res.file_path.empty()) {
            g_telegram.send_photo(chat_id, res.file_path, "Here is your screenshot.");
        }
    }
    else if (action == "camera") {
        g_telegram.answer_callback_query(callback_query_id, "Taking photo...");
        CameraCapture cap;
        auto res = cap.capture("media", 0);
        if (res.success && !res.file_path.empty()) {
            g_telegram.send_photo(chat_id, res.file_path, "Here is your webcam photo.");
        }
    }
    else if (action == "bright_up") {
        std::string ps = "$b = (Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightness).CurrentBrightness + 20; if ($b -gt 100) { $b = 100 }; (Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods).WmiSetBrightness(1,$b)";
        g_executor.run_powershell(ps, 10);
    }
    else if (action == "bright_down") {
        std::string ps = "$b = (Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightness).CurrentBrightness - 20; if ($b -lt 0) { $b = 0 }; (Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods).WmiSetBrightness(1,$b)";
        g_executor.run_powershell(ps, 10);
    }
    else if (action == "lock_confirm") {
        json kb = json::array({
            json::array({
                {{"text", "✅ YES, LOCK"}, {"callback_data", "dock_sys:lock_exec"}},
                {{"text", "❌ CANCEL"}, {"callback_data", "dock_sys:cancel"}}
            })
        });
        g_telegram.edit_message_text(chat_id, message_id, "⚠ **Confirm Lock PC?**", kb);
        g_telegram.answer_callback_query(callback_query_id);
    }
    else if (action == "restart_confirm") {
        json kb = json::array({
            json::array({
                {{"text", "✅ YES, RESTART"}, {"callback_data", "dock_sys:restart_exec"}},
                {{"text", "❌ CANCEL"}, {"callback_data", "dock_sys:cancel"}}
            })
        });
        g_telegram.edit_message_text(chat_id, message_id, "⚠ **Confirm Restart PC?**", kb);
        g_telegram.answer_callback_query(callback_query_id);
    }
    else if (action == "shutdown_confirm") {
        json kb = json::array({
            json::array({
                {{"text", "✅ YES, SHUTDOWN"}, {"callback_data", "dock_sys:shutdown_exec"}},
                {{"text", "❌ CANCEL"}, {"callback_data", "dock_sys:cancel"}}
            })
        });
        g_telegram.edit_message_text(chat_id, message_id, "⚠ **Confirm Shutdown PC?**", kb);
        g_telegram.answer_callback_query(callback_query_id);
    }
    else if (action == "cancel") {
        send_dock(chat_id); // Re-send the dock or re-edit the message back to normal
        g_telegram.answer_callback_query(callback_query_id, "Cancelled");
    }
    else if (action == "lock_exec") {
        LockWorkStation();
        g_telegram.edit_message_text(chat_id, message_id, "🔒 PC Locked.", json());
        g_telegram.answer_callback_query(callback_query_id, "Locked");
    }
    else if (action == "restart_exec") {
        system("shutdown /r /t 0");
        g_telegram.edit_message_text(chat_id, message_id, "🔄 Restarting PC...", json());
        g_telegram.answer_callback_query(callback_query_id, "Restarting");
    }
    else if (action == "shutdown_exec") {
        system("shutdown /s /t 0");
        g_telegram.edit_message_text(chat_id, message_id, "⚡ Shutting down PC...", json());
        g_telegram.answer_callback_query(callback_query_id, "Shutting down");
    }
    else {
        g_telegram.answer_callback_query(callback_query_id, "Unknown Action");
    }
}

}
