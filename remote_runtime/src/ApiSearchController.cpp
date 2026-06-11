#include "../include/ApiSearchController.h"
#include "../include/HttpUtils.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace sara::remote::http;

namespace sara::remote::api {

void handle_search_api_request(SOCKET sock, const std::string& req, const std::string& path) {
    if (path == "/workspace/api/search_code") {
        std::string q = url_decode(parse_query_param(req, "q"));
        std::string dir = url_decode(parse_query_param(req, "dir"));
        
        std::string q_lower = q;
        for (char& c : q_lower) c = std::tolower(c);
        if (dir.empty()) dir = ".";

        nlohmann::json results = nlohmann::json::array();
        if (!q.empty()) {
            try {
                for (auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied); 
                     it != std::filesystem::recursive_directory_iterator(); ++it) {
                    
                    auto& p = *it;
                    if (p.is_directory()) {
                        std::string fname = p.path().filename().string();
                        if (fname == ".git" || fname == "node_modules" || fname == "build" || fname == "dist" || fname == "__pycache__" || fname == ".vs" || fname == ".gemini") {
                            it.disable_recursion_pending();
                        }
                        continue;
                    }
                    if (!p.is_regular_file()) continue;
                    
                    std::string ext = p.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".exe" || ext == ".dll" || ext == ".lib" || ext == ".a" || ext == ".o" || ext == ".obj" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".ico" || ext == ".ttf" || ext == ".woff" || ext == ".woff2" || ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".pdf" || ext == ".mp3" || ext == ".mp4" || ext == ".db" || ext == ".sqlite" || ext == ".sqlite3") {
                        continue;
                    }

                    std::ifstream file(p.path(), std::ios::binary);
                    if (!file.is_open()) continue;
                    
                    char buf[1024];
                    file.read(buf, sizeof(buf));
                    int read_bytes = file.gcount();
                    bool is_binary = false;
                    for(int i=0; i<read_bytes; i++) {
                        if (buf[i] == '\0') { is_binary = true; break; }
                    }
                    if (is_binary) continue;
                    
                    file.clear();
                    file.seekg(0);
                    
                    std::string line;
                    int line_no = 1;
                    while (std::getline(file, line)) {
                        std::string line_lower = line;
                        for (char& c : line_lower) c = std::tolower(c);
                        
                        if (line_lower.find(q_lower) != std::string::npos) {
                            nlohmann::json match;
                            std::string rel_path = std::filesystem::relative(p.path(), dir).string();
                            std::replace(rel_path.begin(), rel_path.end(), '\\', '/');
                            match["file"] = rel_path;
                            match["line"] = line_no;
                            match["content"] = line;
                            results.push_back(match);
                            if (results.size() >= 150) break;
                        }
                        line_no++;
                    }
                    if (results.size() >= 150) break;
                }
            } catch (...) {}
        }
        send_http(sock, 200, "application/json", results.dump());
    } else {
        send_404(sock);
    }
}

} // namespace sara::remote::api
