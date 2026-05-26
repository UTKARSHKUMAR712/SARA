#include "../include/PermissionManager.h"
#include "../include/ActionValidator.h"
#include "../include/Logger.h"
#include <algorithm>

namespace sara {

PermissionManager& PermissionManager::instance() {
    static PermissionManager inst;
    return inst;
}

bool PermissionManager::check_action(const std::string& action,
    const std::string& target,
    const std::string& source,
    const json& parameters)
{
    auto& validator = ActionValidator::instance();
    auto validation = validator.validate(action, target, parameters);

    if (!validation.valid) {
        Logger::instance().warning("Permission denied: " + validation.reason);
        return false;
    }

    if (validation.risk == RiskLevel::LOW) {
        return true;
    }

    if (validation.risk == RiskLevel::MEDIUM) {
        if (source == "telegram" || source == "discord") {
            if (approval_cb_ && !approval_cb_(action, target, source)) {
                Logger::instance().warning("Action rejected by approval callback");
                return false;
            }
        }
        return true;
    }

    if (validation.risk == RiskLevel::HIGH) {


        if (approval_cb_ && !approval_cb_(action, target, source)) {
            Logger::instance().warning("HIGH risk action rejected: " + action);
            return false;
        }
        Logger::instance().info("HIGH risk action approved: " + action);
        return true;
    }

    return false;
}

bool PermissionManager::is_admin(const std::string& user_id) const {
    return admins_.find(user_id) != admins_.end();
}

void PermissionManager::add_admin(const std::string& user_id) {
    admins_.insert(user_id);
}

void PermissionManager::remove_admin(const std::string& user_id) {
    admins_.erase(user_id);
}

bool PermissionManager::is_user_whitelisted(const std::string& platform,
    const std::string& user_id) const
{
    return whitelist_.find(platform + ":" + user_id) != whitelist_.end();
}

void PermissionManager::whitelist_user(const std::string& platform,
    const std::string& user_id)
{
    whitelist_.insert(platform + ":" + user_id);
}

bool PermissionManager::requires_approval(const std::string& action) const {
    auto risk = ActionValidator::instance().get_risk_level(action);
    return risk >= RiskLevel::MEDIUM;
}

}
