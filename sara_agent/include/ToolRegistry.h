#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

struct ActionResult; // forward declaration

// ── Tool Categories ────────────────────────────────────────────────────────────
enum class ToolCategory {
    browser,
    media,
    surveillance,
    productivity,
    system,
    monitoring,
    communication,
    automation,
    internal
};

// ── Tool Definition ────────────────────────────────────────────────────────────
struct ToolDef {
    std::string  name;
    std::string  description;
    ToolCategory category;
    std::string  risk_level;  // "LOW" | "MEDIUM" | "HIGH"

    // The actual executor function: receives params JSON, returns ActionResult JSON
    std::function<json(const json& params)> handler;
};

// ── Tool Registry ─────────────────────────────────────────────────────────────
// Singleton. Dynamic tool dispatch system.
class ToolRegistry {
public:
    static ToolRegistry& instance();

    // Register a tool
    void register_tool(ToolDef def);

    // Execute a tool by name
    json execute(const std::string& name, const json& params);

    // Check if a tool exists
    bool has_tool(const std::string& name) const;

    // Get all tools in a category
    std::vector<ToolDef> get_by_category(ToolCategory cat) const;

    // Get all registered tools
    std::vector<ToolDef> list_all() const;

private:
    ToolRegistry()  = default;
    ~ToolRegistry() = default;

    mutable std::mutex                        mutex_;
    std::unordered_map<std::string, ToolDef> tools_; // name → def
};

} // namespace sara
