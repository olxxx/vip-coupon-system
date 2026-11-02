#include "coupon/ConcreteCoupons.hpp"
#include "user/UserManager.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {

    bool isClose(double left, double right) {
        return std::abs(left - right) < 1e-4;
    }

} // namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] 用户权益管理模块" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 注册与账号唯一性
    {
        user::UserManager manager;

        assert(manager.empty());
        assert(manager.size() == 0);

        auto result = manager.registerCustomer("alice", "123456");
        assert(result.success);
        assert(manager.size() == 1);
        assert(manager.hasCustomer("alice"));

        result = manager.registerCustomer("alice", "654321");
        assert(!result.success);

        result = manager.registerAdmin(
            "alice",
            "admin123",
            core::AdminRole::STAFF
        );
        assert(!result.success);

        result = manager.registerCustomer("", "123456");
        assert(!result.success);

        std::cout << "[PASS] 1. 注册、重复用户名和空用户名校验"
            << std::endl;
    }

    // 2. 登录、密码校验和注销
    {
        user::UserManager manager;
        manager.registerCustomer("alice", "123456");
        manager.registerAdmin(
            "staff",
            "admin123",
            core::AdminRole::STAFF
        );

        auto result = manager.loginCustomer("alice", "wrong");
        assert(!result.success);
        assert(!manager.isLoggedIn());

        result = manager.loginCustomer("alice", "123456");
        assert(result.success);
        assert(manager.isCustomerLoggedIn());
        assert(!manager.isAdminLoggedIn());
        assert(manager.getCurrentUsername() == "alice");
        assert(
            manager.getCurrentAccountType()
            == core::AccountType::CUSTOMER
        );

        result = manager.loginAdmin("staff", "admin123");
        assert(result.success);
        assert(manager.isAdminLoggedIn());
        assert(!manager.isCustomerLoggedIn());
        assert(
            manager.getCurrentAccountType()
            == core::AccountType::ADMIN
        );

        manager.logout();
        assert(!manager.isLoggedIn());
        assert(manager.getCurrentUsername().empty());

        std::cout << "[PASS] 2. 登录、错误密码、会话切换与注销校验"
            << std::endl;
    }

    // 3. 充值、消费、购券扣款和自动升级
    {
        user::CustomerAccount account(
            "buyer",
            "123456",
            "2026-11-02 20:00:00",
            1000.0
        );

        assert(isClose(account.getBalance(), 1000.0));
        assert(isClose(account.getTotalSpent(), 0.0));
        assert(account.getTier() == core::UserTier::V0_NORMAL);
        assert(account.getTransactionCount() == 0);

        account.recharge(500.0, "微信支付");
        assert(isClose(account.getBalance(), 1500.0));
        assert(account.getTransactionCount() == 1);

        account.recharge(0.0);
        account.recharge(-10.0);
        assert(isClose(account.getBalance(), 1500.0));
        assert(account.getTransactionCount() == 1);

        assert(account.deductBalance(200.0, "ORDER_1001"));
        assert(isClose(account.getBalance(), 1300.0));
        assert(isClose(account.getTotalSpent(), 200.0));
        assert(account.getTier() == core::UserTier::V1_BRONZE);

        assert(!account.deductBalance(99999.0, "ORDER_FAILED"));
        assert(isClose(account.getBalance(), 1300.0));
        assert(isClose(account.getTotalSpent(), 200.0));

        assert(account.deductForCoupon(
            50.0,
            "测试优惠券",
            "TPL_TEST"
        ));
        assert(isClose(account.getBalance(), 1250.0));
        assert(isClose(account.getTotalSpent(), 200.0));

        account.recharge(1000.0);
        assert(account.deductBalance(600.0, "ORDER_1002"));
        assert(isClose(account.getTotalSpent(), 800.0));
        assert(account.getTier() == core::UserTier::V2_SILVER);
        assert(isClose(account.getNextTierThreshold(), 2000.0));
        assert(isClose(account.getAmountNeededForNextTier(), 1200.0));
        assert(isClose(account.getGrowthProgress(), 0.0));

        const auto recent = account.getRecentTransactions(3);
        assert(recent.size() == 3);
        assert(
            recent.front()->getType()
            == user::TransactionType::TIER_UPGRADE
        );

        std::cout << "[PASS] 3. 充值、扣款、购券和自动升级校验"
            << std::endl;
    }

    // 4. 卡券领取、查找和核销
    {
        user::CustomerAccount account(
            "coupon-user",
            "123456",
            "2026-11-02 20:00:00"
        );

        auto coupon = std::make_shared<shop::ThresholdCoupon>(
            "TPL_THRESHOLD_10",
            "满100减10",
            0.0,
            100.0,
            core::ProductCategory::ALL,
            core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME,
            10.0,
            "INST_TEST_0001"
        );

        account.addCoupon(coupon);
        account.addCoupon(coupon);

        assert(account.getCouponCount() == 1);
        assert(account.hasCoupon("INST_TEST_0001"));
        assert(account.getAvailableCouponCount() == 1);

        const auto found = account.findCoupon("INST_TEST_0001");
        assert(found != nullptr);
        assert(found->getName() == "满100减10");

        assert(account.consumeCoupon("INST_TEST_0001"));
        assert(!account.consumeCoupon("INST_TEST_0001"));
        assert(account.getAvailableCouponCount() == 0);
        assert(account.getUsedCoupons().size() == 1);
        assert(found->isUsed());

        std::cout << "[PASS] 4. 卡券添加、去重、查找与核销校验"
            << std::endl;
    }

    // 5. CustomerAccount JSON 往返
    {
        user::CustomerAccount original(
            "json-user",
            "json-password",
            "2026-11-02 20:00:00",
            1000.0
        );

        original.recharge(500.0, "兑换码");
        original.addCoupon(
            std::make_shared<shop::CashCoupon>(
                "TPL_CASH_5",
                "五元立减券",
                0.0,
                5.0,
                core::ProductCategory::ALL,
                core::UserTier::V0_NORMAL,
                core::TimeConstraint::ANYTIME,
                "INST_JSON_0001"
            )
        );
        original.consumeCoupon("INST_JSON_0001");

        nlohmann::json json = original.toJson();
        auto restored = user::CustomerAccount::fromJson(json);

        assert(restored != nullptr);
        assert(restored->getUsername() == original.getUsername());
        assert(isClose(
            restored->getBalance(),
            original.getBalance()
        ));
        assert(isClose(
            restored->getTotalSpent(),
            original.getTotalSpent()
        ));
        assert(restored->getTier() == original.getTier());
        assert(
            restored->getTransactionCount()
            == original.getTransactionCount()
        );
        assert(restored->getCouponCount() == 1);
        assert(restored->getAvailableCouponCount() == 0);
        assert(restored->hasCoupon("INST_JSON_0001"));
        assert(
            restored->findCoupon("INST_JSON_0001")->isUsed()
        );

        std::cout << "[PASS] 5. CustomerAccount JSON 序列化与恢复校验"
            << std::endl;
    }

    // 6. 管理员权限和安全调级
    {
        user::UserManager manager;

        manager.registerCustomer("target", "123456");

        manager.registerAdmin(
            "staff",
            "staff123",
            core::AdminRole::STAFF
        );

        manager.registerAdmin(
            "root",
            "root123",
            core::AdminRole::SUPER_ADMIN
        );

        assert(manager.loginCustomer("target", "123456").success);

        auto result = manager.updateCustomerTier(
            "target",
            core::UserTier::V2_SILVER
        );
        assert(!result.success);

        assert(manager.loginAdmin("staff", "staff123").success);

        result = manager.updateCustomerTier(
            "target",
            core::UserTier::V2_SILVER
        );
        assert(!result.success);

        assert(manager.loginAdmin("root", "root123").success);

        result = manager.updateCustomerTier(
            "target",
            core::UserTier::V3_GOLD
        );
        assert(result.success);

        const auto target = manager.findCustomer("target");
        assert(target != nullptr);
        assert(target->getTier() == core::UserTier::V3_GOLD);

        std::cout << "[PASS] 6. 运营管理员与超级管理员权限校验"
            << std::endl;
    }

    // 7. UserManager JSON 全量往返
    {
        user::UserManager original;

        original.registerCustomer("alice", "123456");
        original.registerAdmin(
            "root",
            "root123",
            core::AdminRole::SUPER_ADMIN
        );

        original.loginAdmin("root", "root123");
        assert(original.updateCustomerTier(
            "alice",
            core::UserTier::V2_SILVER
        ).success);

        nlohmann::json json = original.toJson();
        user::UserManager restored = user::UserManager::fromJson(json);

        assert(restored.getCustomerCount() == 1);
        assert(restored.getAdminCount() == 1);
        assert(restored.size() == 2);
        assert(!restored.isLoggedIn());

        const auto alice = restored.findCustomer("alice");
        const auto root = restored.findAdmin("root");

        assert(alice != nullptr);
        assert(root != nullptr);
        assert(alice->getTier() == core::UserTier::V2_SILVER);
        assert(
            root->getRole()
            == core::AdminRole::SUPER_ADMIN
        );
        assert(root->canModifyUserTier());
        assert(root->canResetSystem());

        std::cout << "[PASS] 7. UserManager JSON 全量恢复校验"
            << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有用户模块测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}