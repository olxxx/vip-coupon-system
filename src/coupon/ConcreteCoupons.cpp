#include "coupon/ConcreteCoupons.hpp"
#include "common/Types.hpp"
#include <sstream>
#include <iomanip>

namespace shop
{

// ============================================================================
// 1. 经典满减券 (ThresholdCoupon)
// ============================================================================

double ThresholdCoupon::calculateDiscount(double eligible_amount) const
{
    if (!core::isAmountGTE(eligible_amount, min_spend)) return 0.0;
    return std::min(reduce_amount, eligible_amount);
}

std::string ThresholdCoupon::getRuleDescription() const
{
    std::ostringstream oss;
    oss << "满 " << std::fixed << std::setprecision(2) << min_spend << " 减 " << std::fixed << std::setprecision(2) << reduce_amount;
    return oss.str();
}

nlohmann::json ThresholdCoupon::toJson() const
{
    nlohmann::json j = toJsonBase();
    j["type"] = "THRESHOLD";
    j["reduce_amount"] = reduce_amount;
    return j;
}

std::unique_ptr<Coupon> ThresholdCoupon::fromJson(const nlohmann::json& j)
{
    if (j.value("type", "") != "THRESHOLD") return nullptr;
    return std::make_unique<ThresholdCoupon>(
        j["template_id"],
        j["name"],
        j["cost"],
        j["min_spend"],
        static_cast<core::ProductCategory>(j["target_cat"]),
        static_cast<core::UserTier>(j["min_tier"]),
        static_cast<core::TimeConstraint>(j["time_limit"]),
        j["reduce_amount"],
        j["instance_id"]
    );
}

// ============================================================================
// 2. 百分比折扣券 (DiscountCoupon)
// ============================================================================

double DiscountCoupon::calculateDiscount(double eligible_amount) const
{
    if (!core::isAmountGTE(eligible_amount, min_spend)) return 0.0;
    double discount = eligible_amount * (1.0 - discount_rate);
    if (max_discount > 1e-4) discount = std::min(discount, max_discount);
    return discount;
}

std::string DiscountCoupon::getRuleDescription() const
{
    std::ostringstream oss;
    oss << "满 " << std::fixed << std::setprecision(2) << min_spend << " 享 " << std::setprecision(2) << (discount_rate * 10.0) << " 折";
    if (hasMaxDiscount()) oss << " 封顶 " << std::fixed << std::setprecision(2) << max_discount << "元";
    return oss.str();
}

nlohmann::json DiscountCoupon::toJson() const
{
    nlohmann::json j = toJsonBase();
    j["type"] = "DISCOUNT";
    j["discount_rate"] = discount_rate;
    j["max_discount"] = max_discount;
    return j;
}

std::unique_ptr<Coupon> DiscountCoupon::fromJson(const nlohmann::json& j)
{
    if (j.value("type", "") != "DISCOUNT") return nullptr;
    return std::make_unique<DiscountCoupon>(
        j["template_id"],
        j["name"],
        j["cost"],
        j["min_spend"],
        j["discount_rate"],
        static_cast<core::ProductCategory>(j["target_cat"]),
        static_cast<core::UserTier>(j["min_tier"]),
        static_cast<core::TimeConstraint>(j["time_limit"]),
        j["max_discount"],
        j["instance_id"]
    );
}

// ============================================================================
// 3. 无门槛立减券 (CashCoupon)
// ============================================================================

double CashCoupon::calculateDiscount(double eligible_amount) const
{
    return std::min(cash_amount, eligible_amount);
}

std::string CashCoupon::getRuleDescription() const
{
    std::ostringstream oss;
    oss << "无门槛立减 " << std::fixed << std::setprecision(2) << cash_amount << "元";
    return oss.str();
}

nlohmann::json CashCoupon::toJson() const
{
    nlohmann::json j = toJsonBase();
    j["type"] = "CASH";
    j["cash_amount"] = cash_amount;
    return j;
}

std::unique_ptr<Coupon> CashCoupon::fromJson(const nlohmann::json& j)
{
    if (j.value("type", "") != "CASH") return nullptr;
    return std::make_unique<CashCoupon>(
        j["template_id"],
        j["name"],
        j["cost"],
        j["cash_amount"],
        static_cast<core::ProductCategory>(j["target_cat"]),
        static_cast<core::UserTier>(j["min_tier"]),
        static_cast<core::TimeConstraint>(j["time_limit"]),
        j["instance_id"]
    );
}

} // namespace shop
