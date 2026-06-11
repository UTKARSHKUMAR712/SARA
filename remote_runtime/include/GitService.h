#pragma once

#include <string>

namespace remote_runtime {

class GitService {
public:
    static GitService& getInstance() {
        static GitService instance;
        return instance;
    }

    struct ExecutionResult {
        bool success;
        std::string output;
    };

    // Low-level command execution
    ExecutionResult execute_git_command(const std::string& workspace_path, const std::string& git_args);

    // High-level operations returning JSON strings
    std::string is_git_repo(const std::string& workspace_path);
    std::string current_branch(const std::string& workspace_path);
    std::string create_branch(const std::string& workspace_path, const std::string& name);
    std::string switch_branch(const std::string& workspace_path, const std::string& name);
    std::string commit(const std::string& workspace_path, const std::string& message);
    std::string add_file(const std::string& workspace_path, const std::string& path);
    std::string unstage_file(const std::string& workspace_path, const std::string& path);
    std::string restore_file(const std::string& workspace_path, const std::string& path);
    std::string push(const std::string& workspace_path);
    std::string pull(const std::string& workspace_path);
    std::string recent_commits(const std::string& workspace_path);
    std::string status_summary(const std::string& workspace_path);
    std::string remote_info(const std::string& workspace_path);

private:
    GitService() = default;
    ~GitService() = default;

    bool is_safe_branch_name(const std::string& name);
    bool is_safe_path(const std::string& path);
    std::string parse_git_error(const std::string& output);
    std::string escape_quotes(const std::string& str);
};

} // namespace remote_runtime
