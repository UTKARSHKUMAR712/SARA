#include "../include/MediaDock.h"
#include "../include/TelegramGateway.h"
#include "../include/WinAPIExecutor.h"
#include "../include/Logger.h"

extern sara::TelegramGateway g_telegram;
extern sara::WinAPIExecutor g_executor;

namespace sara {

void MediaDock::send_dock(const std::string& chat_id) {
    json inline_keyboard = json::array({
        json::array({
            {{"text", "⏪ 10s"}, {"callback_data", "dock_media:seek_back"}},
            {{"text", "⏯ Play/Pause"}, {"callback_data", "dock_media:play_pause"}},
            {{"text", "10s ⏩"}, {"callback_data", "dock_media:seek_fwd"}}
        }),
        json::array({
            {{"text", "🔉 Vol -"}, {"callback_data", "dock_media:vol_down"}},
            {{"text", "🔇 Mute"}, {"callback_data", "dock_media:mute"}},
            {{"text", "🔊 Vol +"}, {"callback_data", "dock_media:vol_up"}}
        })
    });

    g_telegram.send_inline_keyboard(chat_id, "🎵 **MEDIA DOCK**", inline_keyboard);
}

void MediaDock::handle_action(const std::string& chat_id, const std::string& action, const std::string& callback_query_id) {
    Logger::instance().info("MediaDock action: " + action);

    if (action == "play_pause") {
        g_executor.send_keys("{VK_MEDIA_PLAY_PAUSE}");
    }
    else if (action == "seek_fwd") {
        g_executor.send_keys("L"); // YouTube shortcut
    }
    else if (action == "seek_back") {
        g_executor.send_keys("J"); // YouTube shortcut
    }
    else if (action == "vol_up") {
        g_executor.send_keys("{VK_VOLUME_UP}{VK_VOLUME_UP}{VK_VOLUME_UP}{VK_VOLUME_UP}{VK_VOLUME_UP}");
    }
    else if (action == "vol_down") {
        g_executor.send_keys("{VK_VOLUME_DOWN}{VK_VOLUME_DOWN}{VK_VOLUME_DOWN}{VK_VOLUME_DOWN}{VK_VOLUME_DOWN}");
    }
    else if (action == "mute") {
        g_executor.send_keys("{VK_VOLUME_MUTE}");
    }
    else {
        g_telegram.answer_callback_query(callback_query_id, "Unknown Action");
    }
}

}
