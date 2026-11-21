#pragma once
#include "common/Types.hpp"
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace user {

    enum class TransactionType {
        RECHARGE,        // 账户充值
        EXPENSE,         // 购物消费支出
        PURCHASE_COUPON, // 付费购买优惠券支出
        TIER_UPGRADE     // 会员等级跃迁变更
    };//枚举

    inline std::string getTransactionTypeName(TransactionType type) {
        switch (type) {
        case TransactionType::RECHARGE:        return "账户充值";
        case TransactionType::EXPENSE:         return "购物消费";
        case TransactionType::PURCHASE_COUPON: return "购券支出";
        case TransactionType::TIER_UPGRADE:    return "等级跃迁";
        }
        return "未知类型";
    }

    // 账单记录抽象基类
    class TransactionRecord {
    protected:
        std::string record_id;    // 流水号
        std::string timestamp;    // 产生时间（时间戳）
        double amount;            // 变动金额
        TransactionType type;     // 账单记录类型

        nlohmann::json toJsonBase() const {
            return nlohmann::json{
                {"record_id", record_id},
                {"timestamp", timestamp},
                {"amount", amount},
                {"type", static_cast<int>(type)}
            };
        }

    public:
        TransactionRecord(std::string id, std::string time, double amount, TransactionType type)
            : record_id(std::move(id)), timestamp(std::move(time)), amount(amount), type(type) {
        }

        virtual ~TransactionRecord() = default;

        const std::string& getRecordId() const { return record_id; }
        const std::string& getTimestamp() const { return timestamp; }
        double getAmount() const { return amount; }
        TransactionType getType() const { return type; }//读取这条流水记录的类型（枚举类型）
        std::string getTypeName() const { return getTransactionTypeName(type); }

        
        static std::string generateId(const std::string& prefix);// 生成流水号 

        virtual std::string getFormattedText() const = 0; // 纯虚函数，统一账单格式，例如："[账户充值] 2026-10-28 12:00:00 +¥500.00"

        virtual nlohmann::json toJson() const = 0;
    };

    // 派生类 1：充值记录
    class RechargeRecord : public TransactionRecord {
    private:
        std::string channel; // 充值渠道 （"Wechat","支付宝"）
    public:
        RechargeRecord(std::string id, std::string time, double amount, std::string channel);
        const std::string& getChannel() const { return channel; }
        std::string getFormattedText() const override;//重写纯虚getFormattedText
        nlohmann::json toJson() const override;
        static std::unique_ptr<RechargeRecord> fromJson(const nlohmann::json& j);
    };

    // 派生类 2：消费支出记录
    class ExpenseRecord : public TransactionRecord {
    private:
        std::string order_id;      // 关联的订单 ID
        std::string description;   // 消费摘要 (如 "购买商品结算")
    public:
        ExpenseRecord(std::string id, std::string time, double amount, std::string order_id, std::string desc);
        const std::string& getOrderId() const { return order_id; }
        const std::string& getDescription() const { return description; }
        std::string getFormattedText() const override;
        nlohmann::json toJson() const override;
        static std::unique_ptr<ExpenseRecord> fromJson(const nlohmann::json& j);
    };

    // 派生类 3：付费购券支出记录
    class PurchaseCouponRecord : public TransactionRecord {
    private:
        std::string template_id;   // 购买的优惠券模板
        std::string coupon_name;   // 购买的券名称
    public:
        PurchaseCouponRecord(std::string id, std::string time, double amount, std::string tpl_id, std::string cpn_name);
        const std::string& getTemplateId() const { return template_id; }
        const std::string& getCouponName() const { return coupon_name; }
        std::string getFormattedText() const override;
        nlohmann::json toJson() const override;
        static std::unique_ptr<PurchaseCouponRecord> fromJson(const nlohmann::json& j);
    };

    // 派生类 4：会员升级记录
    class UpgradeRecord : public TransactionRecord {
    private:
        std::string old_tier_name; // 原等级名称
        std::string new_tier_name; // 新等级名称
    public:
        UpgradeRecord(std::string id, std::string time, std::string old_tier, std::string new_tier);
        const std::string& getOldTierName() const { return old_tier_name; }
        const std::string& getNewTierName() const { return new_tier_name; }
        std::string getFormattedText() const override;
        nlohmann::json toJson() const override;
        static std::unique_ptr<UpgradeRecord> fromJson(const nlohmann::json& j);
    };

} // namespace user
