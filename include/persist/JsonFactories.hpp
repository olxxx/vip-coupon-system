#pragma once

#include "coupon/Coupon.hpp"
#include "user/TransactionRecord.hpp"
#include <nlohmann/json.hpp>
#include <memory>

namespace persist {

/**
 * @brief 优惠券多态反序列化工厂
 */
class CouponFactory {
public:
    // JSON Schema 鉴别字段："type": "THRESHOLD" | "DISCOUNT" | "CASH"
    static std::shared_ptr<shop::Coupon> fromJson(const nlohmann::json& j);
    static nlohmann::json toJson(const shop::Coupon& coupon);
};

/**
 * @brief 账单流水多态反序列化工厂
 */
class TransactionFactory {
public:
    // JSON Schema 鉴别字段："type": "RECHARGE" | "EXPENSE" | "PURCHASE_COUPON" | "UPGRADE"
    static std::unique_ptr<user::TransactionRecord> fromJson(const nlohmann::json& j);
    static nlohmann::json toJson(const user::TransactionRecord& record);
};

} // namespace persist
