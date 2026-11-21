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

    } // namespace 使用命名空间，明确这是内部辅助函数，其他cpp文件不能调用它

    AdminAccount::AdminAccount(std::string name,
        std::string password,
        std::string creation_time,
        core::AdminRole initial_role)
        : Account(
            std::move(name),
            std::move(password),
            std::move(creation_time)),
        role(initial_role) {
    }//构造函数，move不复制字符串内容

    core::AccountType AdminAccount::getAccountType() const {
        return core::AccountType::ADMIN;
    }//返回枚举值

    std::string AdminAccount::getDisplaySummary() const {
        return getUsername() + " | " + getRoleName();
    }

    core::AdminRole AdminAccount::getRole() const {
        return role;
    }//返回枚举值

    std::string AdminAccount::getRoleName() const {
        return role == core::AdminRole::SUPER_ADMIN
            ? "超级管理员"
            : "运营管理员";
    }

    void AdminAccount::setRole(core::AdminRole new_role) {
        role = new_role;
    }//设置或修改角色

    bool AdminAccount::canModifyUserTier() const {
        return role == core::AdminRole::SUPER_ADMIN;
    }//判断是不是超级管理员——有没有修改用户等级的权限

    bool AdminAccount::canManageInventory() const {
        return true;
    }//都可以管理商品

    bool AdminAccount::canIssueCoupons() const {
        return true;
    }//都可以管理优惠券

    bool AdminAccount::canResetSystem() const {
        return role == core::AdminRole::SUPER_ADMIN;
    }//是否有权限重置系统

    nlohmann::json AdminAccount::toJson() const {
        nlohmann::json json = toJsonBase();//调用基类函数，取得账户共有的字段

        json["role"] = static_cast<int>(role);
        json["role_name"] = getRoleName();
        json["account_type"] = "ADMIN";

        return json;
    }//当前管理员对象转换成一份完整的JSON

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
    }//若成功，返回一个指向AdminAccount的指针

} // namespace user