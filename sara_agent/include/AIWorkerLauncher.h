#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include "../include/ConfigManager.h"  // uses AIProviderConfig defined there

namespace sara {

using json = nlohmann::json;

// ── A single step inside an execution plan ──────────────────────────────────
struct PlanStep {
    std::string action;       // maps to WinAPIExecutor action name
    std::string target;       // optional target (process name, URL, etc.)
    json        parameters;   // optional parameters object
    std::string description;  // human-readable explanation for this step
    int         delay_seconds = 0; // pause BEFORE executing this step
};

// ── Result returned by the AI Planner ────────────────────────────────────────
struct PlannerResult {
    bool        success               = false;
    std::string intent;               // classified intent category
    json        execution_plan;       // ordered array of PlanStep objects (as JSON)
    std::string response_text;        // conversational reply to show the user
    bool        needs_clarification   = false;
    std::string clarification_question;
    bool        needs_continue        = false;
    std::string error;
};

// ── AI Worker Launcher ────────────────────────────────────────────────────────
// Thin transport layer: writes input JSON → spawns parser.py → reads output JSON.
// Python NEVER stays running. Fire-and-forget per request.
class AIWorkerLauncher {
public:
    // Plan a command using the primary provider, fall back to secondary on failure.
    PlannerResult plan_command_with_fallback(
        const std::string&      user_message,
        const AIProviderConfig& primary,
        const AIProviderConfig& fallback,
        const json&             session_context = json::object(),
        const std::string&      memory_context  = "");

    // Plan a command using a single provider.
    PlannerResult plan_command(
        const std::string&      user_message,
        const AIProviderConfig& provider_config,
        const json&             session_context = json::object(),
        const std::string&      memory_context  = "");

    // Send a trivial test prompt to verify the provider works.
    json test_provider(const AIProviderConfig& config);

private:
    PlannerResult run_worker(
        const std::string&      user_message,
        const AIProviderConfig& config,
        const json&             session_context,
        const std::string&      memory_context = "",
        const AIProviderConfig* fast_config    = nullptr);

    bool        write_temp_file(const std::string& path, const std::string& content);
    std::string read_temp_file(const std::string& path);
    std::string get_worker_script_path() const;
};

} // namespace sara
