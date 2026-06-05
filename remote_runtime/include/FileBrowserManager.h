#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <windows.h>

namespace sara::remote {

// FileBrowserManager — launches filebrowser.exe as a localhost-only child process.
//
// File Browser listens on 127.0.0.1:9090 ONLY. It is never exposed to the internet.
// All external access goes through TerminalHttpServer's reverse proxy at /files/*.
//
// Launched as:
//   filebrowser.exe -r <root_dir> -a 127.0.0.1 -p 9090 --baseurl /files --noauth
//                   -d <db_path> --log stdout
//
// Singleton — one File Browser process per SARA instance.
class FileBrowserManager {
public:
    static FileBrowserManager& instance() {
        static FileBrowserManager s;
        return s;
    }

    // Ensure filebrowser.exe exists in dir — downloads from GitHub if missing.
    // Returns path to filebrowser.exe, or empty string on failure.
    static std::string ensure_filebrowser(const std::string& dir);

    // Configure the manager with settings (does not start the process)
    void configure(int port, const std::string& root_dir, const std::string& db_path);

    // Start filebrowser.exe using configured settings.
    // Safe to call multiple times (no-op if already running).
    bool start();

    void stop();

    bool is_running() const { return running_; }

    // Check if the child process is still alive (not just the running_ flag).
    bool is_process_alive() const;

    int port() const { return port_; }

private:
    FileBrowserManager() = default;
    ~FileBrowserManager() { stop(); }

    HANDLE proc_handle_  = INVALID_HANDLE_VALUE;
    std::atomic<bool> running_{false};
    int port_            = 9090;
    std::string root_dir_;
    std::string db_path_;
};

} // namespace sara::remote
