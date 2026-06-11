#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <windows.h>
#include "../../remote_runtime/include/CloudflaredManager.h"

namespace sara {

class MeshCentralManager {
public:
    static MeshCentralManager& instance() {
        static MeshCentralManager s;
        return s;
    }

    bool start();
    void stop();
    void request_cloudflare_tunnel();
    bool is_running() const { return running_; }

    std::string get_status() const;
    std::string get_cf_url() const;
    std::string get_lan_url() const;
    std::string get_last_error() const { return last_error_; }

    // Synchronously checks and installs dependencies if needed
    bool verify_and_install();

    MeshCentralManager() = default;
    ~MeshCentralManager() { stop(); }

private:
    void process_monitor_loop();

    std::atomic<bool> running_{false};
    HANDLE proc_handle_ = INVALID_HANDLE_VALUE;
    std::thread monitor_thread_;

    mutable std::mutex cf_mutex_;
    sara::remote::CloudflaredManager cf_manager_;
    std::string cf_url_;

    std::string last_error_;
    std::string node_exe_path_;
    std::string mc_dir_;
};

} // namespace sara
