#include "../include/AgentInitializer.h"
#include "../include/Logger.h"
#include "../include/MediaDock.h"
#include "../../plugins/media/media_service.h"
#include <windows.h>
#include <csignal>
#include <winrt/Windows.Foundation.h>

namespace sara {

static AgentInitializer* g_instance = nullptr;
static std::atomic<bool>* g_running_ptr = nullptr;

static void agent_signal_handler(int) {
    if (g_running_ptr) *g_running_ptr = false;
}

AgentInitializer& AgentInitializer::instance() {
    static AgentInitializer inst;
    return inst;
}

AgentInitializer::AgentInitializer() {
    g_instance = this;
}

AgentInitializer::~AgentInitializer() {
    shutdown();
}

bool AgentInitializer::resolve_paths(char* exe_path) {
    char buf[MAX_PATH];
    if (!exe_path) {
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        exe_path = buf;
    }

    exe_dir_ = exe_path;
    auto pos = exe_dir_.find_last_of("\\/");
    if (pos != std::string::npos) exe_dir_ = exe_dir_.substr(0, pos + 1);

    auto parent_pos = exe_dir_.find_last_of("\\/", exe_dir_.size() - 2);
    if (parent_pos != std::string::npos) {
        std::string parent = exe_dir_.substr(0, parent_pos + 1);
        DWORD attr = GetFileAttributesA((parent + "data").c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            exe_dir_ = parent;
    }
    return true;
}

bool AgentInitializer::initialize(int argc, char* argv[]) {
    SetConsoleTitleA("SARA v4.0 — Cognitive Runtime");
    resolve_paths(argv ? argv[0] : nullptr);

    g_running_ptr = &running_;
    signal(SIGINT, agent_signal_handler);
    signal(SIGTERM, agent_signal_handler);

    // Initialize WinRT apartment for GSMTC and other Windows Runtime APIs
    winrt::init_apartment();

    // Initialize Media Session Service (GSMTC)
    sara::media::MediaService::instance().init();

    // Start the 2-second position refresh timer (WinRT-aware thread)
    sara::media::MediaService::instance().start_refresh_timer();

    // Pre-warm MediaDock singleton so GSMTC event callbacks are registered
    (void)sara::MediaDock::instance();

    Logger::instance().info("=== SARA v4.0 AgentInitializer ===");
    return true;
}

void AgentInitializer::run() {
    main_loop();
}

void AgentInitializer::main_loop() {
    while (running_) {
        Sleep(1000);
    }
}

void AgentInitializer::shutdown() {
    if (!running_) return;
    running_ = false;
    Logger::instance().info("AgentInitializer: shutdown complete");
}

} // namespace sara
