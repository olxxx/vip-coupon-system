#include "user/TransactionRecord.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <utility>

namespace user {

    namespace {

        std::string formatAmount(double amount) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2) << amount;
            //把amount变成保留两位小数放到stream流里面
            return stream.str();//取出字符串内容
        }

    } // namespace

    std::string TransactionRecord::generateId(const std::string& prefix) {
        static std::atomic<std::uint64_t> sequence{ 0 };

        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

        std::ostringstream stream;
        stream << prefix
            << "_"
            << milliseconds
            << "_"
            << ++sequence;

        return stream.str();
    }

    // ---------------RechargeRecord:账户的充值记录--------------- 

    RechargeRecord::RechargeRecord(std::string id,
        std::string time,
        double amount,
        std::string channel)
        : TransactionRecord(
            std::move(id),
            std::move(time),
            amount,
            TransactionType::RECHARGE),
        channel(std::move(channel)) {
    }//RechargeRecord的构造函数

    std::string RechargeRecord::getFormattedText() const {
        return "[" + getTypeName() + "] "
            + getTimestamp()
            + " +¥" + formatAmount(getAmount())
            + "（渠道：" + channel + "）";
    }//充值渠道

    nlohmann::json RechargeRecord::toJson() const {
        nlohmann::json json = toJsonBase();
        json["type"] = "RECHARGE";
        json["type_code"] = static_cast<int>(getType());//读取RECHANGE的枚举类型
        json["channel"] = channel;
        return json;
    }//整体来说是序列化，把对象变成JSON的一条充值记录，用于保存

    std::unique_ptr<RechargeRecord> RechargeRecord::fromJson(
        const nlohmann::json& json
    ) {
        try {
            return std::make_unique<RechargeRecord>(
                json.at("record_id").get<std::string>(),
                json.at("timestamp").get<std::string>(),
                json.at("amount").get<double>(),
                json.value("channel", "未知渠道")
            );
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

    // ---------------ExpenseRecord:消费记录---------------

    ExpenseRecord::ExpenseRecord(std::string id,
        std::string time,
        double amount,
        std::string order_id,
        std::string desc)
        : TransactionRecord(
            std::move(id),
            std::move(time),
            amount,
            TransactionType::EXPENSE),
        order_id(std::move(order_id)),
        description(std::move(desc)) {
    }

    std::string ExpenseRecord::getFormattedText() const {
        return "[" + getTypeName() + "] "
            + getTimestamp()
            + " -¥" + formatAmount(getAmount())
            + "（订单号：" + order_id
            + "，摘要：" + description + "）";
    }

    nlohmann::json ExpenseRecord::toJson() const {
        nlohmann::json json = toJsonBase();
        json["type"] = "EXPENSE";
        json["type_code"] = static_cast<int>(getType());
        json["order_id"] = order_id;
        json["description"] = description;
        return json;
    }

    std::unique_ptr<ExpenseRecord> ExpenseRecord::fromJson(
        const nlohmann::json& json
    ) {
        try {
            return std::make_unique<ExpenseRecord>(
                json.at("record_id").get<std::string>(),
                json.at("timestamp").get<std::string>(),
                json.at("amount").get<double>(),
                json.value("order_id", ""),
                json.value("description", "")
            );
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

    // ---------------PurchaseCouponRecord:买优惠券消费记录---------------

    PurchaseCouponRecord::PurchaseCouponRecord(std::string id,
        std::string time,
        double amount,
        std::string tpl_id,
        std::string cpn_name)
        : TransactionRecord(
            std::move(id),
            std::move(time),
            amount,
            TransactionType::PURCHASE_COUPON),
        template_id(std::move(tpl_id)),
        coupon_name(std::move(cpn_name)) {
    }

    std::string PurchaseCouponRecord::getFormattedText() const {
        return "[" + getTypeName() + "] "
            + getTimestamp()
            + " -¥" + formatAmount(getAmount())
            + "（购买优惠券：" + coupon_name
            + "，模板ID：" + template_id + "）";
    }

    nlohmann::json PurchaseCouponRecord::toJson() const {
        nlohmann::json json = toJsonBase();
        json["type"] = "PURCHASE_COUPON";
        json["type_code"] = static_cast<int>(getType());
        json["template_id"] = template_id;
        json["coupon_name"] = coupon_name;
        return json;
    }

    std::unique_ptr<PurchaseCouponRecord> PurchaseCouponRecord::fromJson(
        const nlohmann::json& json
    ) {
        try {
            return std::make_unique<PurchaseCouponRecord>(
                json.at("record_id").get<std::string>(),
                json.at("timestamp").get<std::string>(),
                json.at("amount").get<double>(),
                json.value("template_id", ""),
                json.value("coupon_name", "")
            );
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

    // ---------------UpgradeRecord:升级记录---------------

    UpgradeRecord::UpgradeRecord(std::string id,
        std::string time,
        std::string old_tier,
        std::string new_tier)
        : TransactionRecord(
            std::move(id),
            std::move(time),
            0.0,
            TransactionType::TIER_UPGRADE),
        old_tier_name(std::move(old_tier)),
        new_tier_name(std::move(new_tier)) {
    }

    std::string UpgradeRecord::getFormattedText() const {
        return "[" + getTypeName() + "] "
            + getTimestamp()
            + " 会员跃迁："
            + old_tier_name + " -> " + new_tier_name;
    }

    nlohmann::json UpgradeRecord::toJson() const {
        nlohmann::json json = toJsonBase();
        json["type"] = "UPGRADE";
        json["type_code"] = static_cast<int>(getType());
        json["old_tier"] = old_tier_name;
        json["new_tier"] = new_tier_name;
        return json;
    }

    std::unique_ptr<UpgradeRecord> UpgradeRecord::fromJson(
        const nlohmann::json& json
    ) {
        try {
            return std::make_unique<UpgradeRecord>(
                json.at("record_id").get<std::string>(),
                json.at("timestamp").get<std::string>(),
                json.value("old_tier", ""),
                json.value("new_tier", "")
            );
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

} // namespace user
