#pragma once

#include "common/Types.hpp"
#include "user/Account.hpp"

#include <memory>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace user {

    class AdminAccount : public Account {
    private:
        core::AdminRole role;

    public:
        AdminAccount(std::string username,
            std::string password,
            std::string created_at,
            core::AdminRole role);

        core::AccountType getAccountType() const override;
        std::string getDisplaySummary() const override;

        core::AdminRole getRole() const;
        std::string getRoleName() const;
        void setRole(core::AdminRole new_role);

        bool canModifyUserTier() const;
        bool canManageInventory() const;
        bool canIssueCoupons() const;
        bool canResetSystem() const;

        nlohmann::json toJson() const override;

        static std::shared_ptr<AdminAccount> fromJson(
            const nlohmann::json& json
        );
    };

} // namespace user