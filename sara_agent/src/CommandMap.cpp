#include "../include/CommandMap.h"
#include "../include/Logger.h"
#include <fstream>
#include <algorithm>
#include <filesystem>

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
        add_commands(j);
        Logger::instance().info("CommandMap(" + filepath + "): "
            + std::to_string(exact_entries_.size()) + " exact, "
            + std::to_string(prefix_entries_.size()) + " prefix, "
            + std::to_string(contains_entries_.size()) + " contains");
        return true;
    } catch (const std::exception& e) {
        Logger::instance().err("CommandMap: parse error in " + filepath + " - " + std::string(e.what()));
        return false;
    }
}

bool CommandMap::load_directory(const std::string& dirpath) {
    exact_entries_.clear();
    prefix_entries_.clear();
    contains_entries_.clear();
    int files_loaded = 0;
    try {
        for (auto& entry : std::filesystem::directory_iterator(dirpath)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() != ".json") continue;
            std::ifstream file(path);
            if (!file.is_open()) {
                Logger::instance().warning("CommandMap: cannot open " + path.string());
                continue;
            }
            try {
                json j;
                file >> j;
                if (!j.contains("commands")) continue;
                int before = (int)(exact_entries_.size() + prefix_entries_.size() + contains_entries_.size());
                add_commands(j);
                int after = (int)(exact_entries_.size() + prefix_entries_.size() + contains_entries_.size());
                files_loaded++;
                Logger::instance().debug("CommandMap: loaded " + path.filename().string()
                    + " (" + std::to_string(after - before) + " entries)");
            } catch (const std::exception& e) {
                Logger::instance().err("CommandMap: parse error in " + path.string() + " - " + std::string(e.what()));
            }
        }
    } catch (const std::exception& e) {
        Logger::instance().err("CommandMap: directory error - " + std::string(e.what()));
        return false;
    }
    Logger::instance().info("CommandMap: loaded " + std::to_string(files_loaded) + " files, "
        + std::to_string(exact_entries_.size()) + " exact, "
        + std::to_string(prefix_entries_.size()) + " prefix, "
        + std::to_string(contains_entries_.size()) + " total");
    return files_loaded > 0;
}

MatchResult CommandMap::match(const std::string& text) const {
    std::string key = normalize(text);
    {
        auto it = exact_entries_.find(key);
        if (it != exact_entries_.end())
            return {&it->second, {}};
    }
    for (auto& e : prefix_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.compare(0, pat.size(), pat) == 0) {
            std::string captured = text.substr(e.pattern.size());
            while (!captured.empty() && captured[0] == ' ') captured.erase(0, 1);
            return {&e, captured};
        }
    }
    for (auto& e : contains_entries_) {
        std::string pat = normalize(e.pattern);
        if (key.find(pat) != std::string::npos)
            return {&e, {}};
    }
    return {nullptr, {}};
}

} // namespace sara
