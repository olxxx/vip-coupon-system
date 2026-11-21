#pragma once

#include "common/Types.hpp"
#include "coupon/Coupon.hpp"
#include "user/Account.hpp"
#include "user/TransactionRecord.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

using namespace std;

namespace user {

    class CustomerAccount : public Account {
    private:
        double balance = 0.0;//余额
        double total_spent = 0.0;//总花销
        core::UserTier tier = core::UserTier::V0_NORMAL;//一开始设为普通会员

        vector<std::shared_ptr<shop::Coupon>> coupons;//动态数组+智能指针(shop::Coupon:优惠券基类)
        vector<std::unique_ptr<TransactionRecord>> transactions;//交易流水集合

    public:
        CustomerAccount(std::string username,
            string password,
            string created_at,
            double initial_balance = 1000.0);//构造函数，新注册用户，默认初始余额1000元

        CustomerAccount(std::string username,
            string password,
            string created_at,
            double balance,
            double total_spent,
            core::UserTier tier);//构造函数重载，可以用来恢复存档

        core::AccountType getAccountType() const override;
        std::string getDisplaySummary() const override;
        //重写纯虚函数

        double getBalance() const;
        double getTotalSpent() const;
        core::UserTier getTier() const;
        std::string getTierName() const;
        double getDiscountRate() const;

        void setBalance(double new_balance);
        void setTotalSpent(double new_total_spent);

        void recharge(double amount,
            const string& channel = "系统充值");

        bool deductBalance(double amount,
            const string& order_id);

        bool deductForCoupon(double amount,
            const string& coupon_name,
            const string& template_id);

        bool checkAndPerformUpgrade(std::string& out_upgrade_message);

        void forceSetTier(core::UserTier new_tier);

        double getNextTierThreshold() const;
        double getGrowthProgress() const;
        double getAmountNeededForNextTier() const;
        bool isMaxTier() const;

        void addCoupon(std::shared_ptr<shop::Coupon> coupon);
        bool consumeCoupon(const std::string& instance_id);
        bool removeCoupon(const std::string& instance_id);

        std::shared_ptr<shop::Coupon> findCoupon(
            const std::string& instance_id
        ) const;

        bool hasCoupon(const std::string& instance_id) const;

        const std::vector<std::shared_ptr<shop::Coupon>>&
            getCoupons() const;

        std::vector<std::shared_ptr<shop::Coupon>>
            getAvailableCoupons() const;

        std::vector<std::shared_ptr<shop::Coupon>>
            getUsedCoupons() const;

        std::size_t getCouponCount() const;
        std::size_t getAvailableCouponCount() const;

        void cleanUsedCoupons();
        void clearCoupons();

        void addTransaction(std::unique_ptr<TransactionRecord> record);

        const std::vector<std::unique_ptr<TransactionRecord>>&
            getTransactions() const;

        std::size_t getTransactionCount() const;

        std::vector<const TransactionRecord*>
            getRecentTransactions(std::size_t limit = 10) const;

        nlohmann::json toJson() const override;

        static std::shared_ptr<CustomerAccount> fromJson(
            const nlohmann::json& json
        );
    };
    
} // 公有继承Account
