#include "../include/MeshCentralManager.h"
#include "../include/ConfigManager.h"
#include "../include/Logger.h"
#include <filesystem>
#include <sstream>
#include <iostream>

extern sara::ConfigManager g_config;
extern std::string g_exe_dir;

namespace fs = std::filesystem;

namespace sara {

static std::string run_powershell_sync(const std::string& cmd) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hOutRead = NULL, hOutWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return "";
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;

    PROCESS_INFORMATION pi = {};
    std::string ps_cmd = "powershell.exe -NoProfile -NonInteractive -Command \"" + cmd + "\"";
    std::vector<char> cmd_buf(ps_cmd.begin(), ps_cmd.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return "";
    }

    CloseHandle(hOutWrite);

    std::string output;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hOutRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutRead);

    return output;
}

static std::string get_lan_ip() {
    std::string ip = "127.0.0.1";
    char name[255];
    if (gethostname(name, sizeof(name)) == 0) {
        struct hostent* hostinfo = gethostbyname(name);
        if (hostinfo && hostinfo->h_addr_list[0]) {
            ip = inet_ntoa(*(struct in_addr*)hostinfo->h_addr_list[0]);
        }
    }
    return ip;
}

bool MeshCentralManager::verify_and_install() {
    auto& cfg = g_config.get();
    mc_dir_ = fs::absolute(g_exe_dir).string() + "\\meshcentral";
    std::string node_dir = fs::absolute(g_exe_dir).string() + "\\data\\nodejs";
    
    fs::create_directories(mc_dir_);

    // Check if global Node.js exists AND is compatible (<= v20)
    bool has_global_node = false;
    std::string node_ver = run_powershell_sync("node -v");
    if (!node_ver.empty() && node_ver[0] == 'v') {
        try {
            int major = std::stoi(node_ver.substr(1, 2));
            if (major <= 20) {
                has_global_node = true;
                node_exe_path_ = "node";
            } else {
                sara::Logger::instance().info("[MeshCentralManager] Global Node.js is v" + std::to_string(major) + " (incompatible). Forcing portable v20.");
            }
        } catch (...) {}
    }

    if (!has_global_node) {
        fs::create_directories(node_dir);
        node_exe_path_ = node_dir + "\\node-v20.11.1-win-x64\\node.exe";
        
        if (!fs::exists(node_exe_path_)) {
            sara::Logger::instance().info("[MeshCentralManager] Downloading Portable Node.js...");
            std::string zip_path = node_dir + "\\node.zip";
            std::string dl_cmd = "Invoke-WebRequest -Uri 'https://nodejs.org/dist/v20.11.1/node-v20.11.1-win-x64.zip' -OutFile '" + zip_path + "'; "
                                 "Expand-Archive -Path '" + zip_path + "' -DestinationPath '" + node_dir + "' -Force; "
                                 "Remove-Item -Path '" + zip_path + "'";
            run_powershell_sync(dl_cmd);
            
            if (!fs::exists(node_exe_path_)) {
                last_error_ = "Failed to download/extract Node.js";
                return false;
            }
        }
    }

    std::string mc_module_path = mc_dir_ + "\\node_modules\\meshcentral";
    if (!fs::exists(mc_module_path)) {
        if (!cfg.meshcentral.auto_install) {
            last_error_ = "MeshCentral is not installed and auto_install is disabled.";
            return false;
        }
        sara::Logger::instance().info("[MeshCentralManager] Installing MeshCentral@1.1.26...");
        
        std::string npm_cmd = has_global_node ? "npm" : ("\"" + node_dir + "\\node-v20.11.1-win-x64\\npm.cmd\"");
        std::string install_cmd = "cmd.exe /c \"cd /d \\\"" + mc_dir_ + "\\\" && " + npm_cmd + " install meshcentral@1.1.26\"";
        
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        std::vector<char> cmd_buf(install_cmd.begin(), install_cmd.end());
        cmd_buf.push_back('\0');
        
        if (CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, mc_dir_.c_str(), &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            last_error_ = "Failed to run npm install command.";
            return false;
        }

        if (!fs::exists(mc_module_path)) {
            last_error_ = "Failed to install MeshCentral (directory not found after npm).";
            return false;
        }
    }

    return true;
}

bool MeshCentralManager::start() {
    if (running_) return true;
    auto& cfg = g_config.get();
    if (!cfg.meshcentral.enabled) return false;

    if (!verify_and_install()) {
        return false;
    }

    sara::Logger::instance().info("[MeshCentralManager] Starting MeshCentral Process...");
    
    std::string mc_main = mc_dir_ + "\\node_modules\\meshcentral\\meshcentral.js";
    std::string cmd = "cmd.exe /c \"\"" + node_exe_path_ + "\" \"" + mc_main + "\" --port " + std::to_string(cfg.meshcentral.port) + "\"";

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, mc_dir_.c_str(), &si, &pi)) {
        last_error_ = "CreateProcess failed for MeshCentral";
        return false;
    }

    proc_handle_ = pi.hProcess;
    CloseHandle(pi.hThread);
    running_ = true;

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    monitor_thread_ = std::thread(&MeshCentralManager::process_monitor_loop, this);

    return true;
}

void MeshCentralManager::request_cloudflare_tunnel() {
    auto& cfg = g_config.get();
    if (cfg.meshcentral.cloudflare_tunnel) {
        {
            std::lock_guard<std::mutex> lock(cf_mutex_);
            if (!cf_url_.empty()) {
                // Already running, optionally restart or just keep existing. Let's keep existing.
                return;
            }
        }
        
        sara::Logger::instance().info("[MeshCentralManager] Starting dedicated Cloudflare Tunnel for MeshCentral...");
        // Pass false for kill_zombies to avoid killing the primary SARA tunnel
        cf_manager_.start_quick_tunnel(cfg.meshcentral.port, [this](const std::string& url) {
            std::lock_guard<std::mutex> lock(cf_mutex_);
            cf_url_ = url;
            sara::Logger::instance().info("[MeshCentralManager] Cloudflare URL Ready: " + url);
        }, false, true);
    }
}

void MeshCentralManager::stop() {
    if (!running_) return;
    running_ = false;

    if (proc_handle_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(proc_handle_, 0);
        WaitForSingleObject(proc_handle_, 3000);
        CloseHandle(proc_handle_);
        proc_handle_ = INVALID_HANDLE_VALUE;
    }

    cf_manager_.stop();

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void MeshCentralManager::process_monitor_loop() {
    while (running_) {
        if (proc_handle_ != INVALID_HANDLE_VALUE) {
            DWORD status = 0;
            if (GetExitCodeProcess(proc_handle_, &status) && status != STILL_ACTIVE) {
                sara::Logger::instance().info("[MeshCentralManager] MeshCentral process exited unexpectedly.");
                running_ = false;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

std::string MeshCentralManager::get_status() const {
    if (!running_) return "Stopped";
    return "Running";
}

std::string MeshCentralManager::get_cf_url() const {
    std::lock_guard<std::mutex> lock(cf_mutex_);
    return cf_url_;
}

std::string MeshCentralManager::get_lan_url() const {
    return "https://" + get_lan_ip() + ":" + std::to_string(g_config.get().meshcentral.port);
}

} // namespace sara
