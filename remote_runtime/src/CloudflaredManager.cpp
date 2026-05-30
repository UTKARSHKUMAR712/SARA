#include "../include/CloudflaredManager.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <mutex>
#include <thread>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace sara::remote {

// ── URL extraction helper ──────────────────────────────────────────────────────
// cloudflared logs look like:
//   INF +----------------------------+
//   INF |  https://xxxx.trycloudflare.com |
//   INF +----------------------------+
// or inline: "Your quick Tunnel has been created! Visit it at https://xxxx.trycloudflare.com"
// We search each line for "https://" + ".trycloudflare.com"
static std::string extract_cf_url(const std::string& line) {
    auto pos = line.find("https://");
    if (pos == std::string::npos) pos = line.find("http://");
    if (pos == std::string::npos) return "";

    // Find end of URL (space, |, newline, carriage return, comma, parenthesis)
    auto end = line.find_first_of(" |\r\n\t,)", pos);
    std::string url = (end != std::string::npos) ? line.substr(pos, end - pos) : line.substr(pos);

    // Skip the terms of service URL
    if (url.find("website-terms") != std::string::npos) return "";

    // Must contain trycloudflare.com or be a named tunnel URL (which doesn't usually print a URL, but just in case)
    if (url.find("trycloudflare.com") != std::string::npos ||
        url.find("cloudflare") != std::string::npos) {
        return url;
    }
    return "";
}

// ── Download cloudflared.exe via WinHTTP ──────────────────────────────────────
std::string CloudflaredManager::ensure_cloudflared(const std::string& dir) {
    std::string path = dir + "\\cloudflared.exe";

    // Already exists?
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return path;
    }

    // Download from official release (Windows x64)
    const wchar_t* host    = L"github.com";
    const wchar_t* url_path =
        L"/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-amd64.exe";

    HINTERNET session = WinHttpOpen(L"SARA/4.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return "";

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return ""; }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", url_path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return ""; }

    // Follow redirects
    DWORD flags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &flags, sizeof(flags));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return "";
    }

    // Write to file
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return "";
    }

    char buf[65536];
    DWORD bytes_read = 0;
    while (WinHttpReadData(request, buf, sizeof(buf), &bytes_read) && bytes_read > 0) {
        DWORD written = 0;
        WriteFile(file, buf, bytes_read, &written, nullptr);
    }

    CloseHandle(file);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    // Verify download
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) return "";
    return path;
}

// ── Start process with piped stderr ───────────────────────────────────────────
static bool launch_process(const std::string& cmd,
                            HANDLE& out_proc,
                            HANDLE& out_stderr_read) {
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};

    HANDLE stderr_read = INVALID_HANDLE_VALUE, stderr_write = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) return false;
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0); // don't inherit read end

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdInput   = INVALID_HANDLE_VALUE;
    si.hStdOutput  = stderr_write; // cloudflared logs go to stdout AND stderr
    si.hStdError   = stderr_write;

    PROCESS_INFORMATION pi{};
    std::string cmd_buf = cmd;

    BOOL ok = CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr,
        TRUE, // inherit handles
        CREATE_NO_WINDOW,
        nullptr, nullptr, &si, &pi);

    CloseHandle(stderr_write); // child has this; close our copy

    if (!ok) { CloseHandle(stderr_read); return false; }

    CloseHandle(pi.hThread);
    out_proc        = pi.hProcess;
    out_stderr_read = stderr_read;
    return true;
}

// ── Reader loop ───────────────────────────────────────────────────────────────
void CloudflaredManager::reader_loop(std::function<void(const std::string&)> on_url_ready) {
    char buf[4096];
    DWORD bytes_read = 0;
    std::string line_buf;
    bool url_found = false;

    while (running_) {
        BOOL ok = ReadFile(stderr_read_, buf, sizeof(buf) - 1, &bytes_read, nullptr);
        if (!ok || bytes_read == 0) break;

        buf[bytes_read] = '\0';
        line_buf += buf;

        // Process complete lines
        std::string::size_type nl;
        while ((nl = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, nl);
            line_buf = line_buf.substr(nl + 1);

            // Try to extract URL
            if (!url_found) {
                std::string url = extract_cf_url(line);
                if (!url.empty()) {
                    url_found = true;
                    {
                        std::lock_guard<std::mutex> lock(url_mutex_);
                        tunnel_url_ = url;
                    }
                    if (on_url_ready) on_url_ready(url);
                }
            }
        }
    }
    running_ = false;
}

// ── Kill any lingering cloudflared processes from previous runs ──────────────
static void kill_zombie_cloudflared() {
    // Kill any cloudflared.exe processes that survived a restart.
    // This handles the case where sara_agent was killed but cloudflared
    // (running as an elevated child) was not properly terminated.
    system("taskkill /F /IM cloudflared.exe /T >nul 2>&1");
    Sleep(500);
}

// ── Public API ────────────────────────────────────────────────────────────────
bool CloudflaredManager::start_quick_tunnel(int local_port,
    std::function<void(const std::string& url)> on_url_ready)
{
    // Always stop any existing tunnel first
    stop();
    
    // Kill any zombie cloudflared from previous process instances
    kill_zombie_cloudflared();

    // Find cloudflared.exe in PATH or current dir
    // (ensure_cloudflared should have been called before this)
    std::string cf_path;
    {
        char buf[MAX_PATH];
        if (SearchPathA(nullptr, "cloudflared", ".exe", MAX_PATH, buf, nullptr))
            cf_path = buf;
    }
    if (cf_path.empty()) return false;

    std::string cmd = "\"" + cf_path + "\" tunnel --url http://localhost:" +
                      std::to_string(local_port) + " --no-autoupdate";

    if (!launch_process(cmd, proc_handle_, stderr_read_)) return false;

    running_ = true;
    reader_thread_ = std::thread([this, on_url_ready]() {
        reader_loop(on_url_ready);
    });
    return true;
}

bool CloudflaredManager::start_named_tunnel(const std::string& tunnel_name,
    std::function<void(const std::string& url)> on_url_ready)
{
    // Always stop any existing tunnel first
    stop();
    
    // Kill any zombie cloudflared from previous process instances
    kill_zombie_cloudflared();

    std::string cf_path;
    {
        char buf[MAX_PATH];
        if (SearchPathA(nullptr, "cloudflared", ".exe", MAX_PATH, buf, nullptr))
            cf_path = buf;
    }
    if (cf_path.empty()) return false;

    std::string cmd = "\"" + cf_path + "\" tunnel run " + tunnel_name;
    if (!launch_process(cmd, proc_handle_, stderr_read_)) return false;

    running_ = true;
    reader_thread_ = std::thread([this, on_url_ready]() {
        reader_loop(on_url_ready);
    });
    return true;
}

void CloudflaredManager::stop() {
    if (!running_) return;
    running_ = false;
    if (proc_handle_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(proc_handle_, 0);
        WaitForSingleObject(proc_handle_, 3000);
        CloseHandle(proc_handle_);
        proc_handle_ = INVALID_HANDLE_VALUE;
    }
    if (stderr_read_ != INVALID_HANDLE_VALUE) {
        CloseHandle(stderr_read_);
        stderr_read_ = INVALID_HANDLE_VALUE;
    }
    if (reader_thread_.joinable()) reader_thread_.join();
}

std::string CloudflaredManager::tunnel_url() const {
    std::lock_guard<std::mutex> lock(url_mutex_);
    return tunnel_url_;
}

bool CloudflaredManager::is_tunnel_alive() const {
    if (!running_) return false;
    if (proc_handle_ == INVALID_HANDLE_VALUE) return false;
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(proc_handle_, &exit_code)) return false;
    return exit_code == STILL_ACTIVE;
}

} // namespace sara::remote
