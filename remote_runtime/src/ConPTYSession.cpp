#include "../include/ConPTYSession.h"
#include <stdexcept>
#include <vector>
#include <string>

// ConPTY API — requires Windows SDK >= 1809 (Build 17763)
// No extra .lib needed; ConPTY lives inside kernel32 / conpty.dll

namespace sara::remote {

ConPTYSession::~ConPTYSession() {
    stop();
}

bool ConPTYSession::start(const std::string& shell_cmd,
                           int cols, int rows,
                           bool admin_mode,
                           OutputCallback on_output,
                           ExitCallback   on_exit)
{
    if (running_) return false;

    cols_      = cols;
    rows_      = rows;
    on_output_ = std::move(on_output);
    on_exit_   = std::move(on_exit);

    // ── Create pipes ──────────────────────────────────────────────────────────
    HANDLE pipe_in_read  = INVALID_HANDLE_VALUE;
    HANDLE pipe_out_write= INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    if (!CreatePipe(&pipe_in_read,  &pipe_in_write_, &sa, 0)) return false;
    if (!CreatePipe(&pipe_out_read_, &pipe_out_write, &sa, 0)) {
        CloseHandle(pipe_in_read);
        CloseHandle(pipe_in_write_);
        return false;
    }

    // ── Create ConPTY ─────────────────────────────────────────────────────────
    COORD size{ (SHORT)cols, (SHORT)rows };
    HRESULT hr = CreatePseudoConsole(size, pipe_in_read, pipe_out_write, 0, &hpc_);

    // These ends are now owned by the ConPTY — close our copies
    CloseHandle(pipe_in_read);
    CloseHandle(pipe_out_write);

    if (FAILED(hr)) {
        CloseHandle(pipe_in_write_);
        CloseHandle(pipe_out_read_);
        hpc_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // ── Build STARTUPINFOEX with ConPTY attribute ─────────────────────────────
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<BYTE> attr_buf(attr_size);
    LPPROC_THREAD_ATTRIBUTE_LIST attr_list =
        reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_buf.data());
    InitializeProcThreadAttributeList(attr_list, 1, 0, &attr_size);
    UpdateProcThreadAttribute(attr_list, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hpc_, sizeof(HPCON), nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb        = sizeof(si);
    si.lpAttributeList       = attr_list;
    si.StartupInfo.dwFlags   = STARTF_USESTDHANDLES;

    // Inherit environment from sara_agent.exe (which runs as admin already)
    // If admin_mode is explicitly requested, use current token (already elevated).
    (void)admin_mode; // current process is elevated; child inherits elevation naturally

    // ── Wide-string shell command ─────────────────────────────────────────────
    std::wstring wcmd(shell_cmd.begin(), shell_cmd.end());
    std::vector<wchar_t> cmd_buf(wcmd.begin(), wcmd.end());
    cmd_buf.push_back(L'\0');

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(
        nullptr,            // application name (use cmd_buf)
        cmd_buf.data(),     // command line
        nullptr,            // process security
        nullptr,            // thread security
        FALSE,              // inherit handles (ConPTY handles via attribute list)
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        nullptr,            // inherit parent environment
        nullptr,            // inherit parent CWD
        &si.StartupInfo,
        &pi
    );

    DeleteProcThreadAttributeList(attr_list);

    if (!ok) {
        ClosePseudoConsole(hpc_);
        CloseHandle(pipe_in_write_);
        CloseHandle(pipe_out_read_);
        hpc_ = INVALID_HANDLE_VALUE;
        return false;
    }

    proc_    = pi.hProcess;
    thread_  = pi.hThread;
    running_ = true;

    // ── Start background reader thread ────────────────────────────────────────
    reader_thread_ = std::thread([this]() { reader_loop(); });

    return true;
}

void ConPTYSession::reader_loop() {
    char buf[4096];
    DWORD bytes_read = 0;

    while (running_) {
        BOOL ok = ReadFile(pipe_out_read_, buf, sizeof(buf), &bytes_read, nullptr);
        if (!ok || bytes_read == 0) break;
        if (on_output_) {
            on_output_(std::string(buf, bytes_read));
        }
    }

    // Shell process exited — report exit code
    DWORD exit_code = 0;
    if (proc_ != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(proc_, 3000);
        GetExitCodeProcess(proc_, &exit_code);
    }

    running_ = false;
    if (on_exit_) on_exit_((int)exit_code);
}

void ConPTYSession::write(const std::string& data) {
    if (!running_ || pipe_in_write_ == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(pipe_in_write_, data.data(), (DWORD)data.size(), &written, nullptr);
}

void ConPTYSession::resize(int cols, int rows) {
    if (hpc_ == INVALID_HANDLE_VALUE) return;
    cols_ = cols;
    rows_ = rows;
    COORD size{(SHORT)cols, (SHORT)rows};
    ResizePseudoConsole(hpc_, size);
}

void ConPTYSession::stop() {
    if (!running_) return;
    running_ = false;

    // Terminate shell process
    if (proc_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(proc_, 0);
        WaitForSingleObject(proc_, 2000);
        CloseHandle(proc_);
        CloseHandle(thread_);
        proc_   = INVALID_HANDLE_VALUE;
        thread_ = INVALID_HANDLE_VALUE;
    }

    // Close pipes (this will unblock ReadFile in reader thread)
    if (pipe_in_write_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_in_write_);
        pipe_in_write_ = INVALID_HANDLE_VALUE;
    }
    if (pipe_out_read_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_out_read_);
        pipe_out_read_ = INVALID_HANDLE_VALUE;
    }

    // Close ConPTY handle
    if (hpc_ != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(hpc_);
        hpc_ = INVALID_HANDLE_VALUE;
    }

    if (reader_thread_.joinable()) reader_thread_.join();
}

bool ConPTYSession::is_alive() const {
    if (!running_ || proc_ == INVALID_HANDLE_VALUE) return false;
    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(proc_, &code);
    return code == STILL_ACTIVE;
}

} // namespace sara::remote
