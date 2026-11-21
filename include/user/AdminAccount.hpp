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
            core::AdminRole role);//定义构造函数

        core::AccountType getAccountType() const override;//重写基类Account的纯虚函数
        std::string getDisplaySummary() const override;//同样重写基类的纯虚函数

        core::AdminRole getRole() const;//读取管理员角色，返回枚举值
        std::string getRoleName() const;//返回管理员角色名字
        void setRole(core::AdminRole new_role);//设置或修改管理员角色

        bool canModifyUserTier() const;//
        bool canManageInventory() const;
        bool canIssueCoupons() const;
        bool canResetSystem() const;

        nlohmann::json toJson() const override;//重写

        static std::shared_ptr<AdminAccount> fromJson(
            const nlohmann::json& json
        );//有static，说明调用时不需要先创建对象；std::shared_ptr<AdminAccount>代表一个智能共享指针，指向某个AdminAccount对象
        //作用:将JSON数据恢复成AdminAccount对象
    };

} // namespace user