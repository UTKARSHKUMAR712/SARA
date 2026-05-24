#pragma once
#include <string>
#include <unordered_set>
#include <functional>
#include <nlohmann/json.hpp>

namespace sara {

using json = nlohmann::json;
using ApprovalCallback = std::function<bool(const std::string& action,
    const std::string& target, const std::string& source)>;

class PermissionManager {
public:
    static PermissionManager& instance();

    bool check_action(const std::string& action,
        const std::string& target,
        const std::string& source,
        const json& parameters);

    void set_approval_callback(ApprovalCallback cb) { approval_cb_ = std::move(cb); }

    bool is_admin(const std::string& user_id) const;
    void add_admin(const std::string& user_id);
    void remove_admin(const std::string& user_id);

    bool is_user_whitelisted(const std::string& platform,
        const std::string& user_id) const;
    void whitelist_user(const std::string& platform, const std::string& user_id);

private:
    PermissionManager() = default;
    bool requires_approval(const std::string& action) const;

    std::unordered_set<std::string> admins_;
    std::unordered_set<std::string> whitelist_;
    ApprovalCallback approval_cb_;
};

}
