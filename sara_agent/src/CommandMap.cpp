#include "../include/CommandMap.h"
#include "../include/Logger.h"
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <regex>

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

void CommandMap::add_commands(const json& j) {
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
            } else if (type == "contains") {
                e.match_type = MatchType::contains;
                contains_entries_.push_back(e);
            } else if (type == "regex") {
                e.match_type = MatchType::regex;
                regex_entries_.push_back(e);
            } else {
                e.match_type = MatchType::exact;
                exact_entries_[key] = e;
            }
        }
    }
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
        regex_entries_.clear();
        add_commands(j);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool CommandMap::load_directory(const std::string& dirpath) {
    exact_entries_.clear();
    prefix_entries_.clear();
    contains_entries_.clear();
    regex_entries_.clear();
    int files_loaded = 0;
    try {
        for (auto& entry : std::filesystem::directory_iterator(dirpath)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() != ".json") continue;
            std::ifstream file(path);
            if (!file.is_open()) continue;
            try {
                json j;
                file >> j;
                if (!j.contains("commands")) continue;
                add_commands(j);
                files_loaded++;
            } catch (...) {}
        }
    } catch (...) { return false; }
    Logger::instance().info("CommandMap: loaded " + std::to_string(files_loaded) + " files, "
        + std::to_string(exact_entries_.size()) + " exact, "
        + std::to_string(prefix_entries_.size()) + " prefix, "
        + std::to_string(regex_entries_.size()) + " regex, "
        + std::to_string(contains_entries_.size()) + " total");
    return files_loaded > 0;
}

MatchResult CommandMap::match(const std::string& text) const {
    std::string cleaned = text;
    {
        std::string lower;
        lower.resize(cleaned.size());
        std::transform(cleaned.begin(), cleaned.end(), lower.begin(), ::tolower);
        if (lower.find("please ") == 0) {
            cleaned = cleaned.substr(7);
            lower.resize(cleaned.size());
            std::transform(cleaned.begin(), cleaned.end(), lower.begin(), ::tolower);
        }
        if (lower.size() >= 7 && lower.rfind(" please") == lower.size() - 7)
            cleaned = cleaned.substr(0, cleaned.size() - 7);
    }
    while (!cleaned.empty() && cleaned[0] == ' ') cleaned.erase(0, 1);
    while (!cleaned.empty() && cleaned.back() == ' ') cleaned.pop_back();

    std::string key = normalize(cleaned);
    
    // 1. Exact
    {
        auto it = exact_entries_.find(key);
        if (it != exact_entries_.end())
            return {&it->second, {}};
    }
    
    // 2. Prefix
    for (auto& e : prefix_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.compare(0, pat.size(), pat) == 0) {
            std::string captured = cleaned.substr(e.pattern.size());
            while (!captured.empty() && captured[0] == ' ') captured.erase(0, 1);
            return {&e, captured};
        }
    }

    // 3. Regex
    for (auto& e : regex_entries_) {
        try {
            std::regex re(e.pattern, std::regex_constants::icase);
            std::smatch m;
            if (std::regex_match(cleaned, m, re)) {
                std::string captured = "";
                if (m.size() > 1) {
                    captured = m[1].str();
                    while (!captured.empty() && captured[0] == ' ') captured.erase(0, 1);
                    while (!captured.empty() && captured.back() == ' ') captured.pop_back();
                }
                return {&e, captured};
            }
        } catch (...) {} // Ignore invalid regex
    }
    
    // 4. Contains
    for (auto& e : contains_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.find(pat) != std::string::npos)
            return {&e, {}};
    }
    return {nullptr, {}};
}

} // namespace sara

