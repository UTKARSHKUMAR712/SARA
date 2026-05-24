#include "../include/HotkeyManager.h"
#include "../include/Logger.h"

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
HotkeyManager& HotkeyManager::instance() {
    static HotkeyManager inst;
    return inst;
}

void HotkeyManager::start() {
    if (running_) return;
    running_ = true;
    worker_  = std::thread(&HotkeyManager::message_loop, this);
    Logger::instance().info("HotkeyManager started");
}

void HotkeyManager::stop() {
    if (!running_) return;
    running_ = false;
    // Post WM_QUIT to the message loop thread to unblock GetMessage
    if (hwnd_) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    if (worker_.joinable()) worker_.join();
    Logger::instance().info("HotkeyManager stopped");
}

// ─────────────────────────────────────────────────────────────────────────────
bool HotkeyManager::register_hotkey(int id, UINT modifiers, UINT vk,
                                    HotkeyCallback cb, const std::string& desc)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (hotkeys_.count(id)) {
        Logger::instance().warning("HotkeyManager: ID " + std::to_string(id) + " already registered");
        return false;
    }
    hotkeys_[id] = { id, modifiers, vk, std::move(cb), desc };

    if (hwnd_) {
        // Window exists — register immediately on the message thread
        // RegisterHotKey must be called from the thread that owns the window
        // so we post a user-defined message to the loop
        PostMessageW(hwnd_, WM_USER + 1, (WPARAM)id, 0);
    }
    Logger::instance().info("Hotkey registered: id=" + std::to_string(id)
        + " desc=" + desc);
    return true;
}

bool HotkeyManager::unregister_hotkey(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!hotkeys_.count(id)) return false;
    hotkeys_.erase(id);
    if (hwnd_) UnregisterHotKey(hwnd_, id);
    return true;
}

std::vector<HotkeyDef> HotkeyManager::list_hotkeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HotkeyDef> out;
    for (auto& [id, def] : hotkeys_) out.push_back(def);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
void HotkeyManager::message_loop() {
    // Create a message-only window on this thread
    WNDCLASSA wc    = {};
    wc.lpfnWndProc  = DefWindowProcA;
    wc.hInstance    = GetModuleHandleA(nullptr);
    wc.lpszClassName = "SARA_Hotkey";
    RegisterClassA(&wc);

    hwnd_ = CreateWindowExA(0, "SARA_Hotkey", "", WS_POPUP,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);

    if (!hwnd_) {
        Logger::instance().err("HotkeyManager: failed to create message window");
        return;
    }

    // Register any hotkeys added before the loop started
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, def] : hotkeys_) {
            if (!RegisterHotKey(hwnd_, id, def.modifiers, def.vk)) {
                Logger::instance().warning("HotkeyManager: failed to register hotkey id="
                    + std::to_string(id));
            }
        }
    }

    MSG msg;
    while (running_ && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            int hid = static_cast<int>(msg.wParam);
            HotkeyCallback cb;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = hotkeys_.find(hid);
                if (it != hotkeys_.end()) cb = it->second.callback;
            }
            if (cb) {
                try { cb(hid); }
                catch (const std::exception& e) {
                    Logger::instance().err("Hotkey callback error: " + std::string(e.what()));
                }
            }
        } else if (msg.message == WM_USER + 1) {
            // New hotkey to register
            int new_id = static_cast<int>(msg.wParam);
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = hotkeys_.find(new_id);
            if (it != hotkeys_.end()) {
                RegisterHotKey(hwnd_, new_id, it->second.modifiers, it->second.vk);
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, _] : hotkeys_) UnregisterHotKey(hwnd_, id);
    }
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

} // namespace sara
