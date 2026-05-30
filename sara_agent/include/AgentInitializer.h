#pragma once
#include <string>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

// ── Agent Lifecycle Manager ────────────────────────────────────────────────
// Cleanly encapsulates initialization, main loop, and shutdown.
// Reduces main.cpp from ~1050 lines to ~40 lines.
class AgentInitializer {
public:
    AgentInitializer();
    ~AgentInitializer();

    bool initialize(int argc, char* argv[]);
    void run();
    void shutdown();
    bool is_running() const { return running_; }
    void stop() { running_ = false; }
    std::string exe_dir() const { return exe_dir_; }

    static AgentInitializer& instance();

private:
    bool resolve_paths(char* exe_path);
    void main_loop();

    std::string exe_dir_;
    std::atomic<bool> running_{true};
};

} // namespace sara
