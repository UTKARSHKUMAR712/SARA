#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

enum class MatchType { exact, prefix, contains };

struct CommandEntry {
    std::string action;
    json params;
    MatchType match_type = MatchType::exact;
    std::string pattern;   // original pattern for prefix/contains
};

struct MatchResult {
    const CommandEntry* entry = nullptr;
    std::string captured;   // captured text after prefix match
};

class CommandMap {
public:
    static CommandMap& instance();

    bool load(const std::string& filepath);
    bool load_directory(const std::string& dirpath);
    void add_commands(const json& j);

    MatchResult match(const std::string& text) const;

private:
    CommandMap() = default;

    std::unordered_map<std::string, CommandEntry> exact_entries_;
    std::vector<CommandEntry> prefix_entries_;
    std::vector<CommandEntry> contains_entries_;
};

} // namespace sara
