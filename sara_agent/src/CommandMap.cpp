#include "../include/CommandMap.h"
#include "../include/Logger.h"
#include <fstream>
#include <algorithm>

namespace sara {

CommandMap& CommandMap::instance() {
    static CommandMap inst;
    return inst;
}

static std::string normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c >= 'A' && c <= 'Z')
            out.push_back(c - 'A' + 'a');
        else
            out.push_back(c);
    }
    return out;
}

bool CommandMap::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::instance().warning("CommandMap: cannot open " + filepath);
        return false;
    }
    try {
        json j;
        file >> j;
        exact_entries_.clear();
        prefix_entries_.clear();
        contains_entries_.clear();

        int exact = 0, prefix = 0, contains = 0;
        for (auto& cmd : j["commands"]) {
            std::string type = cmd.value("type", "exact");
            CommandEntry e;
            e.action = cmd.value("action", "");
            e.params = cmd.value("params", json::object());

            for (auto& phrase : cmd["phrases"]) {
                std::string raw = phrase.get<std::string>();
                e.pattern = raw;
                std::string key = normalize(raw);

                if (type == "prefix") {
                    e.match_type = MatchType::prefix;
                    prefix_entries_.push_back(e);
                    prefix++;
                } else if (type == "contains") {
                    e.match_type = MatchType::contains;
                    contains_entries_.push_back(e);
                    contains++;
                } else {
                    e.match_type = MatchType::exact;
                    exact_entries_[key] = e;
                    exact++;
                }
            }
        }
        Logger::instance().info("CommandMap: " + std::to_string(exact) + " exact, "
            + std::to_string(prefix) + " prefix, " + std::to_string(contains) + " contains");
        return true;
    } catch (const std::exception& e) {
        Logger::instance().err("CommandMap: parse error - " + std::string(e.what()));
        return false;
    }
}

MatchResult CommandMap::match(const std::string& text) const {
    std::string key = normalize(text);

    // 1. Exact match (fastest)
    {
        auto it = exact_entries_.find(key);
        if (it != exact_entries_.end())
            return {&it->second, {}};
    }

    // 2. Prefix match (text starts with pattern, capture rest)
    for (auto& e : prefix_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.compare(0, pat.size(), pat) == 0) {
            std::string captured = text.substr(e.pattern.size());
            while (!captured.empty() && captured[0] == ' ') captured.erase(0, 1);
            return {&e, captured};
        }
    }

    // 3. Contains match (keyword inside text)
    for (auto& e : contains_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.find(pat) != std::string::npos)
            return {&e, {}};
    }

    return {nullptr, {}};
}

} // namespace sara
