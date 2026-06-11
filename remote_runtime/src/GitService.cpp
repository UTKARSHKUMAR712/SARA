#include "GitService.h"
#include <windows.h>
#include <iostream>
#include <vector>

namespace remote_runtime {

GitService::ExecutionResult GitService::execute_git_command(const std::string& workspace_path, const std::string& git_args) {
    ExecutionResult result;
    result.success = false;

    if (workspace_path.empty()) {
        result.output = "Workspace path is empty";
        return result;
    }

    std::string full_cmd = "cmd.exe /C \"git " + git_args + "\"";
    
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        result.output = "Failed to create pipe";
        return result;
    }

    SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hChildStd_OUT_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmd_buf(full_cmd.begin(), full_cmd.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, workspace_path.c_str(), &si, &pi)) {
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_OUT_Rd);
        result.output = "CreateProcess failed";
        return result;
    }

    CloseHandle(hChildStd_OUT_Wr);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000); // 60 seconds timeout
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.output = "Command timed out after 60 seconds";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hChildStd_OUT_Rd);
        return result;
    }

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.success = (exitCode == 0);

    DWORD dwRead;
    CHAR chBuf[4096];
    std::string output;
    while (true) {
        bool bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break;
        chBuf[dwRead] = '\0';
        output += chBuf;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStd_OUT_Rd);

    // Trim trailing whitespace/newlines
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' ')) {
        output.pop_back();
    }

    result.output = output;
    return result;
}

std::string GitService::escape_quotes(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '"') {
            escaped += "\\\"";
        } else if (c == '\\') {
            escaped += "\\\\";
        } else if (c == '\n') {
            escaped += "\\n";
        } else if (c == '\r') {
            escaped += "\\r";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

bool GitService::is_safe_branch_name(const std::string& name) {
    if (name.empty() || name[0] == '-') return false;
    for (char c : name) {
        if (c == '&' || c == '|' || c == ';' || c == '<' || c == '>' || c == '`' || c == '$' || c == '(' || c == ')' || c == '\'' || c == '"' || c == '\\' || c == ' ' || c == '\n' || c == '\r') return false;
    }
    return true;
}

bool GitService::is_safe_path(const std::string& path) {
    for (char c : path) {
        if (c == '&' || c == '|' || c == ';' || c == '<' || c == '>' || c == '`' || c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r') return false;
    }
    return true;
}

std::string GitService::parse_git_error(const std::string& output) {
    if (output.find("Authentication failed") != std::string::npos || output.find("could not read Username") != std::string::npos || output.find("Permission denied") != std::string::npos) {
        return "Git authentication failed. Check credentials.";
    }
    if (output.find("no upstream configured") != std::string::npos || output.find("has no upstream branch") != std::string::npos) {
        return "Current branch has no upstream remote.";
    }
    if (output.find("nothing to commit") != std::string::npos || output.find("no changes added to commit") != std::string::npos) {
        return "No staged changes available.";
    }
    if (output.find("Repository not found") != std::string::npos || output.find("not found") != std::string::npos || output.find("does not appear to be a git repository") != std::string::npos) {
        return "Remote repository not accessible.";
    }
    if (output.find("Command timed out") != std::string::npos) {
        return "Git operation timed out after 60 seconds.";
    }
    return escape_quotes(output);
}

std::string GitService::is_git_repo(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "rev-parse --is-inside-work-tree");
    return res.success && res.output == "true" ? "true" : "false";
}

std::string GitService::current_branch(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "branch --show-current");
    return res.success ? "\"" + escape_quotes(res.output) + "\"" : "null";
}

std::string GitService::create_branch(const std::string& workspace_path, const std::string& name) {
    if (!is_safe_branch_name(name)) return "{\"success\":false, \"message\":\"Invalid branch name\"}";
    auto res = execute_git_command(workspace_path, "switch -c \"" + name + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::switch_branch(const std::string& workspace_path, const std::string& name) {
    if (!is_safe_branch_name(name)) return "{\"success\":false, \"message\":\"Invalid branch name\"}";
    auto res = execute_git_command(workspace_path, "switch \"" + name + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::commit(const std::string& workspace_path, const std::string& message) {
    auto res = execute_git_command(workspace_path, "commit -m \"" + escape_quotes(message) + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::add_file(const std::string& workspace_path, const std::string& path) {
    if (!is_safe_path(path)) return "{\"success\":false, \"message\":\"Invalid file path\"}";
    auto res = execute_git_command(workspace_path, "add \"" + escape_quotes(path) + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::unstage_file(const std::string& workspace_path, const std::string& path) {
    if (!is_safe_path(path)) return "{\"success\":false, \"message\":\"Invalid file path\"}";
    auto res = execute_git_command(workspace_path, "restore --staged \"" + escape_quotes(path) + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::restore_file(const std::string& workspace_path, const std::string& path) {
    if (!is_safe_path(path)) return "{\"success\":false, \"message\":\"Invalid file path\"}";
    auto res = execute_git_command(workspace_path, "restore \"" + escape_quotes(path) + "\"");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::push(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "push");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::pull(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "pull");
    return "{\"success\":" + std::string(res.success ? "true" : "false") + ", \"message\":\"" + parse_git_error(res.output) + "\"}";
}

std::string GitService::recent_commits(const std::string& workspace_path) {
    // Return structured JSON for recent commits: Hash, Message, Author, Date
    auto res = execute_git_command(workspace_path, "log -20 --pretty=format:\"%h|%s|%an|%ad\" --date=short");
    if (!res.success) return "[]";

    std::string json = "[";
    size_t start = 0;
    bool first = true;
    while (start < res.output.length()) {
        size_t end = res.output.find('\n', start);
        if (end == std::string::npos) end = res.output.length();
        
        std::string line = res.output.substr(start, end - start);
        start = end + 1;

        size_t p1 = line.find('|');
        size_t p2 = line.find('|', p1 + 1);
        size_t p3 = line.find('|', p2 + 1);

        if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
            std::string hash = line.substr(0, p1);
            std::string msg = line.substr(p1 + 1, p2 - p1 - 1);
            std::string author = line.substr(p2 + 1, p3 - p2 - 1);
            std::string date = line.substr(p3 + 1);

            if (!first) json += ",";
            json += "{\"hash\":\"" + escape_quotes(hash) + "\",\"message\":\"" + escape_quotes(msg) + "\",\"author\":\"" + escape_quotes(author) + "\",\"date\":\"" + escape_quotes(date) + "\"}";
            first = false;
        }
    }
    json += "]";
    return json;
}

std::string GitService::status_summary(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "status --porcelain");
    if (!res.success) return "{\"staged\":[],\"unstaged\":[],\"unpushed\":0}";

    std::string staged_json = "[";
    std::string unstaged_json = "[";
    bool first_staged = true;
    bool first_unstaged = true;

    size_t start = 0;
    while (start < res.output.length()) {
        size_t end = res.output.find('\n', start);
        if (end == std::string::npos) end = res.output.length();
        
        std::string line = res.output.substr(start, end - start);
        start = end + 1;

        if (line.length() >= 3) {
            char c1 = line[0];
            char c2 = line[1];

            std::string file_path = line.substr(3);
            if (!file_path.empty() && file_path.front() == '"' && file_path.back() == '"') {
                file_path = file_path.substr(1, file_path.length() - 2);
            }

            if (c1 != ' ' && c1 != '?') {
                std::string status = (c1 == 'M' ? "M" : (c1 == 'D' ? "D" : "A"));
                if (!first_staged) staged_json += ",";
                staged_json += "{\"path\":\"" + escape_quotes(file_path) + "\",\"status\":\"" + status + "\"}";
                first_staged = false;
            }
            if (c2 != ' ') {
                std::string status = (c2 == 'M' ? "M" : (c2 == 'D' ? "D" : (c2 == '?' ? "U" : "A")));
                if (!first_unstaged) unstaged_json += ",";
                unstaged_json += "{\"path\":\"" + escape_quotes(file_path) + "\",\"status\":\"" + status + "\"}";
                first_unstaged = false;
            }
        }
    }
    staged_json += "]";
    unstaged_json += "]";

    auto unpushed_res = execute_git_command(workspace_path, "rev-list --count @{u}..HEAD");
    int unpushed = 0;
    bool has_upstream = true;
    if (unpushed_res.success && !unpushed_res.output.empty()) {
        try { unpushed = std::stoi(unpushed_res.output); } catch (...) {}
    } else if (unpushed_res.output.find("no upstream configured") != std::string::npos || unpushed_res.output.find("no upstream branch") != std::string::npos) {
        has_upstream = false;
    }

    return "{\"has_upstream\":" + std::string(has_upstream ? "true" : "false") + ",\"unpushed\":" + std::to_string(unpushed) + ",\"staged\":" + staged_json + ",\"unstaged\":" + unstaged_json + "}";
}

std::string GitService::remote_info(const std::string& workspace_path) {
    auto res = execute_git_command(workspace_path, "remote -v");
    if (!res.success) return "[]";

    std::string json = "[";
    size_t start = 0;
    bool first = true;
    while (start < res.output.length()) {
        size_t end = res.output.find('\n', start);
        if (end == std::string::npos) end = res.output.length();
        
        std::string line = res.output.substr(start, end - start);
        start = end + 1;

        if (line.find("(fetch)") != std::string::npos) {
            std::string name, url, type;
            size_t p1 = 0;
            while (p1 < line.length() && (line[p1] == ' ' || line[p1] == '\t')) p1++;
            size_t p2 = p1;
            while (p2 < line.length() && line[p2] != ' ' && line[p2] != '\t') p2++;
            if (p2 > p1) name = line.substr(p1, p2 - p1);

            while (p2 < line.length() && (line[p2] == ' ' || line[p2] == '\t')) p2++;
            size_t p3 = p2;
            while (p3 < line.length() && line[p3] != ' ' && line[p3] != '\t') p3++;
            if (p3 > p2) url = line.substr(p2, p3 - p2);

            if (!name.empty() && !url.empty()) {
                if (!first) json += ",";
                json += "{\"name\":\"" + escape_quotes(name) + "\",\"url\":\"" + escape_quotes(url) + "\"}";
                first = false;
            }
        }
    }
    json += "]";
    return json;
}

} // namespace remote_runtime
