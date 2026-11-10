#include "persist/JsonFactories.hpp"
#include "coupon/ConcreteCoupons.hpp"

namespace persist {

std::shared_ptr<shop::Coupon> CouponFactory::fromJson(const nlohmann::json& j) {
    if (!j.is_object()) {
        return nullptr;
    }

    const std::string type = j.value("type", "");
    std::unique_ptr<shop::Coupon> coupon = nullptr;

    if (type == "THRESHOLD") {
        coupon = shop::ThresholdCoupon::fromJson(j);
    } else if (type == "DISCOUNT") {
        coupon = shop::DiscountCoupon::fromJson(j);
    } else if (type == "CASH") {
        coupon = shop::CashCoupon::fromJson(j);
    }

    if (!coupon) {
        return nullptr;
    }

    return std::shared_ptr<shop::Coupon>(std::move(coupon));
}

nlohmann::json CouponFactory::toJson(const shop::Coupon& coupon) {
    return coupon.toJson();
}

std::unique_ptr<user::TransactionRecord> TransactionFactory::fromJson(const nlohmann::json& j) {
    if (!j.is_object()) {
        return nullptr;
    }

    const std::string type = j.value("type", "");

    if (type == "RECHARGE") {
        return user::RechargeRecord::fromJson(j);
    }
    if (type == "EXPENSE") {
        return user::ExpenseRecord::fromJson(j);
    }
    if (type == "PURCHASE_COUPON") {
        return user::PurchaseCouponRecord::fromJson(j);
    }
    if (type == "UPGRADE") {
        return user::UpgradeRecord::fromJson(j);
    }

    return nullptr;
}

nlohmann::json TransactionFactory::toJson(const user::TransactionRecord& record) {
    return record.toJson();
}

} // namespace persist
