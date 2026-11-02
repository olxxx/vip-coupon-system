#include "user/UserManager.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <exception>
#include <tuple>
#include <utility>

namespace user {

    namespace {

        std::string currentTimestamp() {
            const auto now = std::chrono::system_clock::now();
            const std::time_t raw_time =
                std::chrono::system_clock::to_time_t(now);

            std::tm local_time{};

#ifdef _WIN32
            localtime_s(&local_time, &raw_time);
#else
            localtime_r(&raw_time, &local_time);
#endif

            char buffer[32]{};
            std::strftime(
                buffer,
                sizeof(buffer),
                "%Y-%m-%d %H:%M:%S",
                &local_time
            );

            return buffer;
        }

        bool isValidTier(core::UserTier tier) {
            const int value = static_cast<int>(tier);
            return value >= static_cast<int>(core::UserTier::V0_NORMAL)
                && value <= static_cast<int>(core::UserTier::V5_SUPREME);
        }

    } // namespace

    std::size_t UserManager::size() const {
        return customers.size() + admins.size();
    }

    std::size_t UserManager::getCustomerCount() const {
        return customers.size();
    }

    std::size_t UserManager::getAdminCount() const {
        return admins.size();
    }

    bool UserManager::empty() const {
        return customers.empty() && admins.empty();
    }

    void UserManager::clear() {
        customers.clear();
        admins.clear();
        current_customer.reset();
        current_admin.reset();
    }

    bool UserManager::hasCustomer(
        const std::string& username
    ) const {
        return customers.find(username) != customers.end();
    }

    bool UserManager::hasAdmin(
        const std::string& username
    ) const {
        return admins.find(username) != admins.end();
    }

    bool UserManager::removeCustomer(
        const std::string& username
    ) {
        if (current_customer
            && current_customer->getUsername() == username) {
            current_customer.reset();
        }

        return customers.erase(username) > 0;
    }

    bool UserManager::removeAdmin(
        const std::string& username
    ) {
        if (current_admin
            && current_admin->getUsername() == username) {
            current_admin.reset();
        }

        return admins.erase(username) > 0;
    }

    core::Result UserManager::registerCustomer(
        const std::string& username,
        const std::string& password
    ) {
        if (username.empty()) {
            return core::Result::fail("用户名不能为空");
        }

        if (password.empty()) {
            return core::Result::fail("密码不能为空");
        }

        if (hasCustomer(username) || hasAdmin(username)) {
            return core::Result::fail("用户名已存在");
        }

        auto customer = std::make_shared<CustomerAccount>(
            username,
            password,
            currentTimestamp()
        );

        customers.emplace(username, std::move(customer));

        return core::Result::ok("注册成功");
    }

    core::Result UserManager::registerAdmin(
        const std::string& username,
        const std::string& password,
        core::AdminRole role
    ) {
        if (username.empty()) {
            return core::Result::fail("用户名不能为空");
        }

        if (password.empty()) {
            return core::Result::fail("密码不能为空");
        }

        if (hasCustomer(username) || hasAdmin(username)) {
            return core::Result::fail("用户名已存在");
        }

        auto admin = std::make_shared<AdminAccount>(
            username,
            password,
            currentTimestamp(),
            role
        );

        admins.emplace(username, std::move(admin));

        return core::Result::ok("管理员账号创建成功");
    }

    core::Result UserManager::loginCustomer(
        const std::string& username,
        const std::string& password
    ) {
        const auto customer = findCustomer(username);

        if (!customer) {
            return core::Result::fail("消费者账号不存在");
        }

        if (!customer->verifyPassword(password)) {
            return core::Result::fail("密码错误");
        }

        current_customer = customer;
        current_admin.reset();

        return core::Result::ok("登录成功");
    }

    core::Result UserManager::loginAdmin(
        const std::string& username,
        const std::string& password
    ) {
        const auto admin = findAdmin(username);

        if (!admin) {
            return core::Result::fail("管理员账号不存在");
        }

        if (!admin->verifyPassword(password)) {
            return core::Result::fail("管理员密码错误");
        }

        current_admin = admin;
        current_customer.reset();

        return core::Result::ok("管理员登录成功");
    }

    void UserManager::logout() {
        current_customer.reset();
        current_admin.reset();
    }

    bool UserManager::isLoggedIn() const {
        return isCustomerLoggedIn() || isAdminLoggedIn();
    }

    bool UserManager::isCustomerLoggedIn() const {
        return current_customer != nullptr;
    }

    bool UserManager::isAdminLoggedIn() const {
        return current_admin != nullptr;
    }

    std::shared_ptr<CustomerAccount>
        UserManager::getCurrentCustomer() const {
        return current_customer;
    }

    std::shared_ptr<AdminAccount>
        UserManager::getCurrentAdmin() const {
        return current_admin;
    }

    std::string UserManager::getCurrentUsername() const {
        if (current_customer) {
            return current_customer->getUsername();
        }

        if (current_admin) {
            return current_admin->getUsername();
        }

        return "";
    }

    core::AccountType UserManager::getCurrentAccountType() const {
        if (current_admin) {
            return core::AccountType::ADMIN;
        }

        return core::AccountType::CUSTOMER;
    }

    std::shared_ptr<CustomerAccount>
        UserManager::findCustomer(
            const std::string& username
        ) const {
        const auto found = customers.find(username);
        return found == customers.end()
            ? nullptr
            : found->second;
    }

    std::shared_ptr<AdminAccount>
        UserManager::findAdmin(
            const std::string& username
        ) const {
        const auto found = admins.find(username);
        return found == admins.end()
            ? nullptr
            : found->second;
    }

    std::vector<std::shared_ptr<CustomerAccount>>
        UserManager::getAllCustomers() const {
        std::vector<std::shared_ptr<CustomerAccount>> result;
        result.reserve(customers.size());

        for (const auto& entry : customers) {
            result.push_back(entry.second);
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const auto& left, const auto& right) {
                return left->getUsername() < right->getUsername();
            }
        );

        return result;
    }

    std::vector<std::shared_ptr<AdminAccount>>
        UserManager::getAllAdmins() const {
        std::vector<std::shared_ptr<AdminAccount>> result;
        result.reserve(admins.size());

        for (const auto& entry : admins) {
            result.push_back(entry.second);
        }

        std::sort(
            result.begin(),
            result.end(),
            [](const auto& left, const auto& right) {
                return left->getUsername() < right->getUsername();
            }
        );

        return result;
    }

    core::Result UserManager::updateCustomerTier(
        const std::string& target_username,
        core::UserTier new_tier
    ) {
        if (!current_admin) {
            return core::Result::fail("请先以管理员身份登录");
        }

        if (!current_admin->canModifyUserTier()) {
            return core::Result::fail("当前管理员没有修改会员等级的权限");
        }

        if (!isValidTier(new_tier)) {
            return core::Result::fail("会员等级无效");
        }

        const auto customer = findCustomer(target_username);

        if (!customer) {
            return core::Result::fail("目标消费者账号不存在");
        }

        customer->forceSetTier(new_tier);
        return core::Result::ok("会员等级修改成功");
    }

    void UserManager::addAdmin(
        std::shared_ptr<AdminAccount> admin
    ) {
        if (!admin || admin->getUsername().empty()) {
            return;
        }

        if (hasCustomer(admin->getUsername())
            || hasAdmin(admin->getUsername())) {
            return;
        }

        admins.emplace(admin->getUsername(), std::move(admin));
    }

    const std::unordered_map<
        std::string,
        std::shared_ptr<CustomerAccount>
    >& UserManager::getCustomerMap() const {
        return customers;
    }

    const std::unordered_map<
        std::string,
        std::shared_ptr<AdminAccount>
    >& UserManager::getAdminMap() const {
        return admins;
    }

    void UserManager::loadCustomerDirect(
        std::shared_ptr<CustomerAccount> customer
    ) {
        if (!customer || customer->getUsername().empty()) {
            return;
        }

        if (hasAdmin(customer->getUsername())) {
            return;
        }

        customers[customer->getUsername()] = std::move(customer);
    }

    nlohmann::json UserManager::toJson() const {
        nlohmann::json root;

        nlohmann::json customer_array =
            nlohmann::json::array();

        for (const auto& customer : getAllCustomers()) {
            customer_array.push_back(customer->toJson());
        }

        nlohmann::json admin_array =
            nlohmann::json::array();

        for (const auto& admin : getAllAdmins()) {
            admin_array.push_back(admin->toJson());
        }

        root["customers"] = std::move(customer_array);
        root["admins"] = std::move(admin_array);

        return root;
    }

    bool UserManager::loadFromJson(const nlohmann::json& json) {
        try {
            if (!json.is_object()) {
                return false;
            }

            std::unordered_map<
                std::string,
                std::shared_ptr<CustomerAccount>
            > loaded_customers;

            std::unordered_map<
                std::string,
                std::shared_ptr<AdminAccount>
            > loaded_admins;

            const nlohmann::json customer_array =
                json.value(
                    "customers",
                    nlohmann::json::array()
                );

            for (const auto& item : customer_array) {
                auto customer = CustomerAccount::fromJson(item);

                if (!customer
                    || customer->getUsername().empty()) {
                    return false;
                }

                const std::string username =
                    customer->getUsername();

                if (loaded_customers.find(username)
                    != loaded_customers.end()
                    || loaded_admins.find(username)
                    != loaded_admins.end()) {
                    return false;
                }

                loaded_customers.emplace(
                    username,
                    std::move(customer)
                );
            }

            const nlohmann::json admin_array =
                json.value(
                    "admins",
                    nlohmann::json::array()
                );

            for (const auto& item : admin_array) {
                auto admin = AdminAccount::fromJson(item);

                if (!admin || admin->getUsername().empty()) {
                    return false;
                }

                const std::string username =
                    admin->getUsername();

                if (loaded_customers.find(username)
                    != loaded_customers.end()
                    || loaded_admins.find(username)
                    != loaded_admins.end()) {
                    return false;
                }

                loaded_admins.emplace(
                    username,
                    std::move(admin)
                );
            }

            customers = std::move(loaded_customers);
            admins = std::move(loaded_admins);
            current_customer.reset();
            current_admin.reset();

            return true;
        }
        catch (const std::exception&) {
            return false;
        }
    }

    UserManager UserManager::fromJson(
        const nlohmann::json& json
    ) {
        UserManager manager;
        manager.loadFromJson(json);
        return manager;
    }

} // namespace user