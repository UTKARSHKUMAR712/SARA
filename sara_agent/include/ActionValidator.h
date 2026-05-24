#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;

enum class RiskLevel {
    LOW,
    MEDIUM,
    HIGH,
    UNKNOWN
};

struct ValidationResult {
    bool valid = false;
    RiskLevel risk = RiskLevel::UNKNOWN;
    std::string reason;
};

class ActionValidator {
public:
    ValidationResult validate(const std::string& action,
        const std::string& target, const json& parameters);

    RiskLevel get_risk_level(const std::string& action) const;
    bool is_action_allowed(const std::string& action) const;

    void add_allowed_action(const std::string& action);
    void remove_allowed_action(const std::string& action);

    static ActionValidator& instance();

private:
    ActionValidator();
    RiskLevel classify_action(const std::string& action) const;
    std::vector<std::string> allowed_actions_;
};

}
