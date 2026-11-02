#include "user/TransactionRecord.hpp"

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
        return std::make_unique<RechargeRecord>(
            json.at("record_id").get<std::string>(),
            //json.at("record_id")是读取id的JSON值，.get<std::string>()把它重新转化成字符串
            json.at("timestamp").get<std::string>(),
            json.at("amount").get<double>(),
            json.at("channel").get<std::string>()
        );//反序列化，把保存成 JSON 的一条充值记录，重新变成一个对象
    }

    //toJson和fromJson两个函数起到保存数据的作用，当程序结束后，数据可以保存到user.json中，下次启动时，fromJson将JSON恢复成对象

    //---------------ExpenseRecord:账户的消费记录-----------------

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
            + "（订单：" + order_id
            + "；摘要：" + description + "）";
    }//消费记录

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
        return std::make_unique<ExpenseRecord>(
            json.at("record_id").get<std::string>(),
            json.at("timestamp").get<std::string>(),
            json.at("amount").get<double>(),
            json.at("order_id").get<std::string>(),
            json.at("description").get<std::string>()
        );
    }

    // ---------------- PurchaseCouponRecord:购买优惠券 ----------------

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
            + "（优惠券：" + coupon_name
            + "；模板：" + template_id + "）";
    }//购买优惠券记录

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
        return std::make_unique<PurchaseCouponRecord>(
            json.at("record_id").get<std::string>(),
            json.at("timestamp").get<std::string>(),
            json.at("amount").get<double>(),
            json.at("template_id").get<std::string>(),
            json.at("coupon_name").get<std::string>()
        );
    }

    // ---------------- UpgradeRecord:会员等级升级 ----------------

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
            + " " + old_tier_name
            + " → " + new_tier_name;
    }//升级记录（等级跃升）

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
        return std::make_unique<UpgradeRecord>(
            json.at("record_id").get<std::string>(),
            json.at("timestamp").get<std::string>(),
            json.at("old_tier").get<std::string>(),
            json.at("new_tier").get<std::string>()
        );
    }

} // namespace user