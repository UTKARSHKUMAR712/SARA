#include "../include/AIWorkerLauncher.h"
#include "../include/Logger.h"
#include "../include/ToolRegistry.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
PlannerResult AIWorkerLauncher::plan_command_with_fallback(
    const std::string&      user_message,
    const AIProviderConfig& primary,
    const AIProviderConfig& fallback,
    const json&             session_context,
    const std::string&      memory_context)
{
    auto result = run_worker(user_message, primary, session_context, memory_context);
    if (!result.success && !fallback.provider.empty()) {
        Logger::instance().warning("Primary AI failed, trying fallback: " + fallback.provider);
        result = run_worker(user_message, fallback, session_context, memory_context);
    }
    return result;
}

PlannerResult AIWorkerLauncher::plan_command(
    const std::string&      user_message,
    const AIProviderConfig& provider_config,
    const json&             session_context,
    const std::string&      memory_context)
{
    return run_worker(user_message, provider_config, session_context, memory_context);
}

// ─────────────────────────────────────────────────────────────────────────────
PlannerResult AIWorkerLauncher::run_worker(
    const std::string&      user_message,
    const AIProviderConfig& config,
    const json&             session_context,
    const std::string&      memory_context)
{
    PlannerResult result;

    std::string script_path = get_worker_script_path();
    if (!std::filesystem::exists(script_path)) {
        result.error = "AI worker script not found: " + script_path;
        Logger::instance().err(result.error);
        return result;
    }

    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string tmp         = std::filesystem::temp_directory_path().string();
    std::string input_file  = tmp + "\\sara_in_"  + std::to_string(ts) + ".json";
    std::string output_file = tmp + "\\sara_out_" + std::to_string(ts) + ".json";

    // ── Build input payload ──────────────────────────────────────────────────
    json input;
    input["user_message"]    = user_message;
    input["memory_context"]  = memory_context;
    input["session_context"] = session_context;
    input["config"]["provider"]        = config.provider;
    input["config"]["model"]           = config.model;
    input["config"]["api_key"]         = config.api_key;
    input["config"]["endpoint"]        = config.endpoint;
    input["config"]["max_tokens"]      = config.max_tokens;
    input["config"]["temperature"]     = config.temperature;
    input["config"]["timeout_seconds"] = config.timeout_seconds;
    input["tools"]                     = ToolRegistry::instance().build_tool_definitions();

    if (!write_temp_file(input_file, input.dump())) {
        result.error = "Failed to write AI worker input file";
        return result;
    }

    // ── Spawn Python worker ──────────────────────────────────────────────────
    std::string cmd = "python \"" + script_path + "\" --input \""
                    + input_file + "\" --output \"" + output_file + "\"";

    STARTUPINFOA si  = { sizeof(si) };
    si.dwFlags       = STARTF_USESHOWWINDOW;
    si.wShowWindow   = SW_HIDE;
    PROCESS_INFORMATION pi;
    char buf[8192];
    strncpy_s(buf, cmd.c_str(), _TRUNCATE);

    if (!CreateProcessA(nullptr, buf, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        result.error = "Failed to start AI worker process";
        std::filesystem::remove(input_file);
        return result;
    }

    DWORD timeout_ms = static_cast<DWORD>(config.timeout_seconds) * 1000 + 5000;
    WaitForSingleObject(pi.hProcess, timeout_ms);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::filesystem::remove(input_file);

    // ── Read and parse output ────────────────────────────────────────────────
    if (!std::filesystem::exists(output_file)) {
        result.error = "AI worker produced no output (exit=" + std::to_string(exit_code) + ")";
        return result;
    }

    std::string raw = read_temp_file(output_file);
    std::filesystem::remove(output_file);

    try {
        auto j = json::parse(raw);
        result.success               = j.value("success", false);
        result.intent                = j.value("intent", "unknown");
        result.response_text         = j.value("response_text", "");
        result.needs_clarification   = j.value("needs_clarification", false);
        result.clarification_question = j.value("clarification_question", "");

        if (j.contains("execution_plan") && j["execution_plan"].is_array()) {
            result.execution_plan = j["execution_plan"];
        } else {
            result.execution_plan = json::array();
        }

        if (!result.success && j.contains("error")) {
            result.error = j["error"].get<std::string>();
        } else if (exit_code != 0 && result.error.empty()) {
            result.error = "AI worker exit code: " + std::to_string(exit_code);
        }
    } catch (const std::exception& e) {
        result.error = std::string("Failed to parse worker output: ") + e.what();
        Logger::instance().err(result.error + " | raw=" + raw.substr(0, 300));
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
json AIWorkerLauncher::test_provider(const AIProviderConfig& config) {
    Logger::instance().info("AI test: provider=" + config.provider
        + " model=" + config.model);
    auto r = run_worker("Say exactly: SARA online.", config, json::object());
    json j;
    j["success"]  = r.success;
    j["response"] = r.response_text;
    j["error"]    = r.error;
    if (r.success) Logger::instance().info("AI test OK: " + r.response_text);
    else           Logger::instance().err("AI test FAIL: " + r.error);
    return j;
}

// ─────────────────────────────────────────────────────────────────────────────
bool AIWorkerLauncher::write_temp_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

std::string AIWorkerLauncher::read_temp_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string AIWorkerLauncher::get_worker_script_path() const {
    namespace fs = std::filesystem;
    auto exe_dir = fs::current_path();
    auto candidate = exe_dir / "ai_workers" / "parser.py";
    if (fs::exists(candidate)) return candidate.string();
    candidate = exe_dir.parent_path() / "ai_workers" / "parser.py";
    if (fs::exists(candidate)) return candidate.string();
    return (fs::path("ai_workers") / "parser.py").string();
}

} // namespace sara
