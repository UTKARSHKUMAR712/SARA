#pragma once
#include "MCPTransport.h"
#include <nlohmann/json.hpp>
#include <future>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace sara {

using json = nlohmann::json;

class MCPClient {
public:
    MCPClient(std::shared_ptr<MCPTransport> transport);
    ~MCPClient();

    bool start(const std::string& cmd, const std::vector<std::string>& args);
    void stop();

    bool initialize();
    std::vector<json> list_tools();
    json call_tool(const std::string& name, const json& arguments);

private:
    void handle_message(const std::string& msg);
    int get_next_id();

    std::shared_ptr<MCPTransport> transport_;
    int request_id_ = 1;
    
    std::mutex promises_mutex_;
    std::unordered_map<int, std::promise<json>> pending_promises_;
};

} // namespace sara
