#include "plugins/mcp/MCPTransport.h"
#include "Logger.h"
#include <windows.h>
#include <thread>
#include <sstream>

namespace sara {

MCPStdioTransport::MCPStdioTransport() {}

MCPStdioTransport::~MCPStdioTransport() {
    stop();
}

void MCPStdioTransport::set_on_message(std::function<void(const std::string&)> cb) {
    on_message_ = cb;
}

void MCPStdioTransport::set_on_error(std::function<void(const std::string&)> cb) {
    on_error_ = cb;
}

bool MCPStdioTransport::start(const std::string& cmd, const std::vector<std::string>& args) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return false;
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return false;
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return false;

    std::string full_cmd = "cmd.exe /C " + cmd;
    for (const auto& arg : args) {
        full_cmd += " \"" + arg + "\"";
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));

    siStartInfo.cb = sizeof(STARTUPINFOA);
    siStartInfo.hStdError = hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.hStdInput = hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    char cmdLine[8192];
    strncpy_s(cmdLine, full_cmd.c_str(), _TRUNCATE);

    bool bSuccess = CreateProcessA(NULL, 
        cmdLine,       
        NULL,          
        NULL,          
        TRUE,          
        CREATE_NO_WINDOW,
        NULL,          
        NULL,          
        &siStartInfo,  
        &piProcInfo);  

    if (!bSuccess) {
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_IN_Rd);
        CloseHandle(hChildStd_IN_Wr);
        if (on_error_) on_error_("Failed to CreateProcess for MCP Server: " + full_cmd);
        return false;
    }

    hProcess_ = piProcInfo.hProcess;
    hThread_ = piProcInfo.hThread;
    
    CloseHandle(hChildStd_OUT_Wr);
    CloseHandle(hChildStd_IN_Rd);

    hStdOutRead_ = hChildStd_OUT_Rd;
    hStdInWrite_ = hChildStd_IN_Wr;

    running_ = true;

    auto t = new std::thread(&MCPStdioTransport::read_loop, this);
    read_thread_ = t;

    return true;
}

void MCPStdioTransport::stop() {
    running_ = false;
    if (hProcess_) {
        TerminateProcess(hProcess_, 0);
        CloseHandle(hProcess_);
        hProcess_ = nullptr;
    }
    if (hThread_) {
        CloseHandle(hThread_);
        hThread_ = nullptr;
    }
    if (hStdInWrite_) {
        CloseHandle(hStdInWrite_);
        hStdInWrite_ = nullptr;
    }
    if (hStdOutRead_) {
        CloseHandle(hStdOutRead_);
        hStdOutRead_ = nullptr;
    }
    if (read_thread_) {
        auto t = static_cast<std::thread*>(read_thread_);
        if (t->joinable()) {
            t->detach(); // Don't block on exit
        }
        delete t;
        read_thread_ = nullptr;
    }
}

bool MCPStdioTransport::send(const std::string& json_message) {
    if (!hStdInWrite_ || !running_) return false;
    
    std::string to_send = json_message + "\n";
    DWORD dwWritten;
    bool bSuccess = WriteFile(hStdInWrite_, to_send.c_str(), to_send.length(), &dwWritten, NULL);
    return bSuccess;
}

void MCPStdioTransport::read_loop() {
    DWORD dwRead;
    CHAR chBuf[4096];
    std::string buffer;

    while (running_) {
        bool bSuccess = ReadFile(hStdOutRead_, chBuf, sizeof(chBuf), &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break;
        
        buffer.append(chBuf, dwRead);
        
        // Process newlines
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty() && on_message_) {
                on_message_(line);
            }
        }
    }
    running_ = false;
    if (on_error_) on_error_("MCP transport read loop terminated.");
}

} // namespace sara
