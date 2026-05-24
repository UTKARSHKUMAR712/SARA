#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

// Builds the correct JSON command for each /sp subcommand
// and sends it through SpotifyWS.
// Returns a human-readable reply string for Telegram.

class SpotifyCommands {
public:
    static std::string dispatch(const std::string& sub, const std::string& args);

private:
    static std::string send(const json& cmd);
    static std::string not_connected();
};

} // namespace sara
