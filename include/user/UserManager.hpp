#pragma once

#include "common/Result.hpp"
#include "common/Types.hpp"
#include "user/AdminAccount.hpp"
#include "user/CustomerAccount.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace user {

    class UserManager {
    private:
        std::unordered_map<
            std::string,
            std::shared_ptr<CustomerAccount>
        > customers;

        std::unordered_map<
            std::string,
            std::shared_ptr<AdminAccount>
        > admins;

        std::shared_ptr<CustomerAccount> current_customer;
        std::shared_ptr<AdminAccount> current_admin;

    public:
        UserManager() = default;

        std::size_t size() const;
        std::size_t getCustomerCount() const;
        std::size_t getAdminCount() const;
        bool empty() const;
        void clear();

        bool hasCustomer(const std::string& username) const;
        bool hasAdmin(const std::string& username) const;

        bool removeCustomer(const std::string& username);
        bool removeAdmin(const std::string& username);

        core::Result registerCustomer(
            const std::string& username,
            const std::string& password
        );

        core::Result registerAdmin(
            const std::string& username,
            const std::string& password,
            core::AdminRole role
        );

        core::Result loginCustomer(
            const std::string& username,
            const std::string& password
        );

        core::Result loginAdmin(
            const std::string& username,
            const std::string& password
        );

        void logout();

        bool isLoggedIn() const;
        bool isCustomerLoggedIn() const;
        bool isAdminLoggedIn() const;

        std::shared_ptr<CustomerAccount>
            getCurrentCustomer() const;

        std::shared_ptr<AdminAccount>
            getCurrentAdmin() const;

        std::string getCurrentUsername() const;
        core::AccountType getCurrentAccountType() const;

        std::shared_ptr<CustomerAccount> findCustomer(
            const std::string& username
        ) const;

        std::shared_ptr<AdminAccount> findAdmin(
            const std::string& username
        ) const;

        std::vector<std::shared_ptr<CustomerAccount>>
            getAllCustomers() const;

        std::vector<std::shared_ptr<AdminAccount>>
            getAllAdmins() const;

        core::Result updateCustomerTier(
            const std::string& target_username,
            core::UserTier new_tier
        );

        void addAdmin(std::shared_ptr<AdminAccount> admin);

        const std::unordered_map<
            std::string,
            std::shared_ptr<CustomerAccount>
        >& getCustomerMap() const;

        const std::unordered_map<
            std::string,
            std::shared_ptr<AdminAccount>
        >& getAdminMap() const;

        void loadCustomerDirect(
            std::shared_ptr<CustomerAccount> customer
        );

        nlohmann::json toJson() const;
        bool loadFromJson(const nlohmann::json& json);

        static UserManager fromJson(const nlohmann::json& json);
    };

} // namespace user