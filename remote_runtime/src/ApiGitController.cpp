#include "../include/ApiGitController.h"
#include "../include/HttpUtils.h"
#include "../include/GitService.h"

using namespace sara::remote::http;

namespace sara::remote::api {

void handle_git_api_request(SOCKET sock, const std::string& req, const std::string& git_action, const std::string& workspace_path) {
    auto& git = remote_runtime::GitService::getInstance();

    if (git_action == "status") {
        std::string is_repo = git.is_git_repo(workspace_path);
        if (is_repo == "false") {
            auto version_res = git.execute_git_command(workspace_path, "--version");
            bool git_installed = (version_res.success || version_res.output.find("git version") != std::string::npos);
            send_http(sock, 200, "application/json", "{\"is_repo\":false, \"git_installed\":" + std::string(git_installed ? "true" : "false") + "}");
        } else {
            std::string branch = git.current_branch(workspace_path);
            std::string summary = git.status_summary(workspace_path);
            std::string remote = git.remote_info(workspace_path);
            
            std::string json = "{";
            json += "\"is_repo\":true,";
            json += "\"branch\":" + branch + ",";
            json += "\"summary\":" + summary + ",";
            json += "\"remote\":" + remote;
            json += "}";
            send_http(sock, 200, "application/json", json);
        }
    } else if (git_action == "log") {
        std::string logs = git.recent_commits(workspace_path);
        send_http(sock, 200, "application/json", logs);
    } else if (git_action == "branch") {
        std::string name = url_decode(parse_query_param(req, "name"));
        std::string create = url_decode(parse_query_param(req, "create"));
        std::string res;
        if (create == "true") res = git.create_branch(workspace_path, name);
        else res = git.switch_branch(workspace_path, name);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "commit") {
        std::string message = url_decode(parse_query_param(req, "message"));
        std::string res = git.commit(workspace_path, message);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "add") {
        std::string path_val = url_decode(parse_query_param(req, "path"));
        if (path_val.empty()) path_val = ".";
        std::string res = git.add_file(workspace_path, path_val);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "unstage") {
        std::string path_val = url_decode(parse_query_param(req, "path"));
        if (path_val.empty()) path_val = ".";
        std::string res = git.unstage_file(workspace_path, path_val);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "restore") {
        std::string path_val = url_decode(parse_query_param(req, "path"));
        if (path_val.empty()) path_val = ".";
        std::string res = git.restore_file(workspace_path, path_val);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "push") {
        std::string res = git.push(workspace_path);
        send_http(sock, 200, "application/json", res);
    } else if (git_action == "pull") {
        std::string res = git.pull(workspace_path);
        send_http(sock, 200, "application/json", res);
    } else {
        send_http(sock, 404, "text/plain", "Unknown git action");
    }
}

} // namespace sara::remote::api
