#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <windows.h>

namespace sara {

using HotkeyCallback = std::function<void(int id)>;

struct HotkeyDef {
    int             id;
    UINT            modifiers; // MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN
    UINT            vk;        // Virtual key code
    HotkeyCallback  callback;
    std::string     description;
};

// ── Hotkey Manager ────────────────────────────────────────────────────────────
// Singleton. Registers global hotkeys via RegisterHotKey() on a dedicated
// message-loop thread. Fires callbacks when hotkeys are pressed.
// Does NOT block the main thread.
class HotkeyManager {
public:
    static HotkeyManager& instance();

    void start();
    void stop();
    bool is_running() const { return running_; }

    // Register a global hotkey. Returns false if already registered.
    bool register_hotkey(int id, UINT modifiers, UINT vk,
                         HotkeyCallback cb, const std::string& desc = "");

    // Unregister a hotkey by its ID.
    bool unregister_hotkey(int id);

    // List all registered hotkeys
    std::vector<HotkeyDef> list_hotkeys() const;

private:
    HotkeyManager()  = default;
    ~HotkeyManager() { stop(); }

    void message_loop();

    std::atomic<bool>  running_{false};
    std::thread        worker_;
    mutable std::mutex mutex_;
    HWND               hwnd_   = nullptr;

    std::unordered_map<int, HotkeyDef> hotkeys_; // id → def
};

} // namespace sara
