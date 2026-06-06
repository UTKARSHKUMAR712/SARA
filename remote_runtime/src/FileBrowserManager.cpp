#include "../include/FileBrowserManager.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <fstream>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace sara::remote {

// ── ZIP extraction helper (Shell32 / IShellDispatch) ─────────────────────────
// filebrowser releases a single .exe for Windows (not a zip since v2.x)
// The Windows release asset is: filebrowser-windows-amd64.exe (direct exe)
// Download URL pattern:
//   https://github.com/filebrowser/filebrowser/releases/latest/download/windows-amd64-filebrowser.zip
// The zip contains a single file: filebrowser.exe
// We use Windows built-in zip support via ShellAPI (XP+).

static bool extract_zip_to_dir(const std::string& zip_path, const std::string& out_dir) {
    // Use PowerShell to extract — simple and universally available on Windows
    std::string cmd =
        "powershell.exe -NoProfile -NonInteractive -Command \""
        "Expand-Archive -LiteralPath '" + zip_path + "' -DestinationPath '" + out_dir + "' -Force"
        "\"";
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;
    si.hStdOutput = INVALID_HANDLE_VALUE;
    si.hStdError = INVALID_HANDLE_VALUE;

    std::string buf = cmd;
    BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr,
        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    WaitForSingleObject(pi.hProcess, 60000); // 60s timeout
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return exit_code == 0;
}

// ── WinHTTP download helper ───────────────────────────────────────────────────
static bool winhttp_download(const wchar_t* host, const wchar_t* url_path,
                              const std::string& dest_path) {
    HINTERNET session = WinHttpOpen(L"SARA/4.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;

    // Set longer timeouts for large files
    WinHttpSetTimeouts(session, 30000, 30000, 60000, 120000);

    HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) { WinHttpCloseHandle(session); return false; }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", url_path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect); WinHttpCloseHandle(session); return false;
    }

    DWORD flags = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &flags, sizeof(flags));

    // Disable SSL certificate verification errors (for corporate proxies)
    DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                           SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                           SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &security_flags, sizeof(security_flags));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    // Check HTTP status
    DWORD status = 0; DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    HANDLE file = CreateFileA(dest_path.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        return false;
    }

    char buf[65536]; DWORD bytes_read = 0;
    while (WinHttpReadData(request, buf, sizeof(buf), &bytes_read) && bytes_read > 0) {
        DWORD written = 0;
        WriteFile(file, buf, bytes_read, &written, nullptr);
    }

    CloseHandle(file);
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    return GetFileAttributesA(dest_path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// ── Public API ────────────────────────────────────────────────────────────────

std::string FileBrowserManager::ensure_filebrowser(const std::string& dir) {
    std::string exe_path = dir + "\\filebrowser.exe";

    // Already downloaded?
    if (GetFileAttributesA(exe_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return exe_path;
    }

    // Create dir if it doesn't exist
    CreateDirectoryA(dir.c_str(), nullptr);

    // Download the Windows amd64 zip from GitHub releases
    // URL: https://github.com/filebrowser/filebrowser/releases/latest/download/windows-amd64-filebrowser.zip
    std::string zip_path = dir + "\\filebrowser.zip";

    bool ok = winhttp_download(
        L"github.com",
        L"/filebrowser/filebrowser/releases/latest/download/windows-amd64-filebrowser.zip",
        zip_path);

    if (!ok) return "";

    // Extract zip to dir
    if (!extract_zip_to_dir(zip_path, dir)) return "";

    // Clean up zip
    DeleteFileA(zip_path.c_str());

    // Verify exe exists
    if (GetFileAttributesA(exe_path.c_str()) == INVALID_FILE_ATTRIBUTES) return "";
    return exe_path;
}

void FileBrowserManager::configure(int port, const std::string& root_dir, const std::string& db_path) {
    port_ = port;
    root_dir_ = root_dir;
    db_path_ = db_path;
}

bool FileBrowserManager::start() {
    if (running_) return true;
    if (db_path_.empty()) return false; // not configured

    // Find filebrowser.exe in PATH or runtime dir
    std::string fb_path;
    {
        char buf[MAX_PATH];
        if (SearchPathA(nullptr, "filebrowser", ".exe", MAX_PATH, buf, nullptr))
            fb_path = buf;
    }
    if (fb_path.empty()) return false;

    // To ensure filebrowser uses our desired port and baseurl even if a DB already exists,
    // we use `config init` (ignores if exists) and `config set`.
    std::string init_cmd = "\"" + fb_path + "\" config init -d \"" + db_path_ + "\" > NUL 2>&1";
    system(init_cmd.c_str());

    std::string config_cmd = "\"" + fb_path + "\" config set"
        " -a 127.0.0.1 -p " + std::to_string(port_) +
        " --baseURL /files"
        " --auth.method proxy --auth.header X-Remote-User"
        " -d \"" + db_path_ + "\" > NUL 2>&1";
    system(config_cmd.c_str());

    // Create a default admin user so --noauth works without 404 errors
    std::string add_user_cmd = "\"" + fb_path + "\" users add admin \"adminadminadmin\" --perm.admin -d \"" + db_path_ + "\" > NUL 2>&1";
    system(add_user_cmd.c_str());

    // Fix Windows quote escape bug: if root_dir ends with a backslash, \" will escape the quote!
    // To pass a literal backslash at the end of a quoted string, we must double it.
    std::string safe_root = root_dir_;
    if (!safe_root.empty() && safe_root.back() == '\\') {
        safe_root.push_back('\\'); // "C:\" becomes "C:\\"
    }

    // Build command for actual run:
    std::string cmd =
        "\"" + fb_path + "\""
        " -r \"" + safe_root + "\""
        " -a 127.0.0.1"
        " -p " + std::to_string(port_) +
        " --baseURL /files"
        " -d \"" + db_path_ + "\"";

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    STARTUPINFOA si{}; si.cb = sizeof(si);
    // Don't set STARTF_USESTDHANDLES if we don't have valid handles
    si.dwFlags = 0;

    PROCESS_INFORMATION pi{};
    std::string cmd_buf = cmd;
    BOOL ok = CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr,
        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    if (!ok) return false;

    CloseHandle(pi.hThread);
    proc_handle_ = pi.hProcess;
    running_     = true;

    // Give the daemon a half-second to bind to the port so immediate /files proxy works.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    return true;
}

void FileBrowserManager::stop() {
    if (!running_) return;
    running_ = false;
    if (proc_handle_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(proc_handle_, 0);
        WaitForSingleObject(proc_handle_, 3000);
        CloseHandle(proc_handle_);
        proc_handle_ = INVALID_HANDLE_VALUE;
    }
}

bool FileBrowserManager::is_process_alive() const {
    if (!running_ || proc_handle_ == INVALID_HANDLE_VALUE) return false;
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(proc_handle_, &exit_code)) return false;
    return exit_code == STILL_ACTIVE;
}

} // namespace sara::remote
