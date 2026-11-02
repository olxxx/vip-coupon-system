#include "user/AdminAccount.hpp"

#include <exception>
#include <utility>

namespace user {

    namespace {

        core::AdminRole parseRole(int value) {
            if (value == static_cast<int>(core::AdminRole::SUPER_ADMIN)) {
                return core::AdminRole::SUPER_ADMIN;
            }

            return core::AdminRole::STAFF;
        }

    } // namespace

    AdminAccount::AdminAccount(std::string name,
        std::string password,
        std::string creation_time,
        core::AdminRole initial_role)
        : Account(
            std::move(name),
            std::move(password),
            std::move(creation_time)),
        role(initial_role) {
    }

    core::AccountType AdminAccount::getAccountType() const {
        return core::AccountType::ADMIN;
    }

    std::string AdminAccount::getDisplaySummary() const {
        return getUsername() + " | " + getRoleName();
    }

    core::AdminRole AdminAccount::getRole() const {
        return role;
    }

    std::string AdminAccount::getRoleName() const {
        return role == core::AdminRole::SUPER_ADMIN
            ? "超级管理员"
            : "运营管理员";
    }

    void AdminAccount::setRole(core::AdminRole new_role) {
        role = new_role;
    }

    bool AdminAccount::canModifyUserTier() const {
        return role == core::AdminRole::SUPER_ADMIN;
    }

    bool AdminAccount::canManageInventory() const {
        return true;
    }

    bool AdminAccount::canIssueCoupons() const {
        return true;
    }

    bool AdminAccount::canResetSystem() const {
        return role == core::AdminRole::SUPER_ADMIN;
    }

    nlohmann::json AdminAccount::toJson() const {
        nlohmann::json json = toJsonBase();

        json["role"] = static_cast<int>(role);
        json["role_name"] = getRoleName();
        json["account_type"] = "ADMIN";

        return json;
    }

    std::shared_ptr<AdminAccount> AdminAccount::fromJson(
        const nlohmann::json& json
    ) {
        try {
            return std::make_shared<AdminAccount>(
                json.at("username").get<std::string>(),
                json.at("password_hash").get<std::string>(),
                json.at("created_at").get<std::string>(),
                parseRole(json.value("role", 0))
            );
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

} // namespace user