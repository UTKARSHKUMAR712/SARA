#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <windows.h>

namespace sara::remote {

// CloudflaredManager — launches cloudflared quick tunnel and captures the URL.
//
// Quick tunnel (no account needed):
//   cloudflared tunnel --url http://localhost:9081
//   → outputs URL to stderr: https://xxxx.trycloudflare.com
//
// Named tunnel (with cloudflare account, stable URL):
//   cloudflared tunnel run <name>
//   → same URL every time
class CloudflaredManager {
public:
    static CloudflaredManager& instance() {
        static CloudflaredManager s;
        return s;
    }

    // Start quick tunnel pointing at local terminal server.
    // on_url_ready is called when the tunnel URL is captured (from another thread).
    // Returns true if cloudflared process was started.
    bool start_quick_tunnel(int local_port,
                            std::function<void(const std::string& url)> on_url_ready,
                            bool kill_zombies = true,
                            bool is_https = false);

    // Start named tunnel (requires cloudflare account + tunnel configured).
    bool start_named_tunnel(const std::string& tunnel_name,
                            std::function<void(const std::string& url)> on_url_ready);

    void stop();
    bool is_running() const { return running_; }
    
    // Check if the cloudflared process is still alive (not just running_ flag).
    bool is_tunnel_alive() const;

    // Get the captured tunnel URL (empty if not yet captured).
    std::string tunnel_url() const;

    // Ensure cloudflared.exe exists in the given dir — downloads if missing.
    // Returns path to cloudflared.exe, or empty on failure.
    static std::string ensure_cloudflared(const std::string& dir);

    CloudflaredManager() = default;
    ~CloudflaredManager() { stop(); }

private:
    void reader_loop(std::function<void(const std::string&)> on_url_ready);

    HANDLE proc_handle_     = INVALID_HANDLE_VALUE;
    HANDLE stderr_read_     = INVALID_HANDLE_VALUE;
    std::atomic<bool> running_{false};
    std::thread reader_thread_;

    mutable std::mutex url_mutex_;
    std::string tunnel_url_;
};

} // namespace sara::remote
