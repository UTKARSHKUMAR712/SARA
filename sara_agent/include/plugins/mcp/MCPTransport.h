#pragma once
#include <string>
#include <vector>
#include <functional>

namespace sara {

class MCPTransport {
public:
    virtual ~MCPTransport() = default;
    virtual bool start(const std::string& cmd, const std::vector<std::string>& args) = 0;
    virtual void stop() = 0;
    virtual bool send(const std::string& json_message) = 0;
    virtual void set_on_message(std::function<void(const std::string&)> cb) = 0;
    virtual void set_on_error(std::function<void(const std::string&)> cb) = 0;
};

class MCPStdioTransport : public MCPTransport {
public:
    MCPStdioTransport();
    ~MCPStdioTransport() override;

    bool start(const std::string& cmd, const std::vector<std::string>& args) override;
    void stop() override;
    bool send(const std::string& json_message) override;
    void set_on_message(std::function<void(const std::string&)> cb) override;
    void set_on_error(std::function<void(const std::string&)> cb) override;

private:
    void read_loop();

    void* hProcess_ = nullptr;
    void* hThread_ = nullptr;
    void* hStdInWrite_ = nullptr;
    void* hStdOutRead_ = nullptr;

    std::function<void(const std::string&)> on_message_;
    std::function<void(const std::string&)> on_error_;
    bool running_ = false;
    void* read_thread_ = nullptr;
};

} // namespace sara
