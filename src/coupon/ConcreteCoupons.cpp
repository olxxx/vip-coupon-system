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
    try
    {
        if (j.value("type", "") != "THRESHOLD")
        {
            return nullptr;
        }

        std::string template_id = j.at("template_id").get<std::string>();
        std::string name = j.at("name").get<std::string>();
        double min_spend = j.at("min_spend").get<double>();
        double reduce_amount = j.at("reduce_amount").get<double>();

        double cost = j.value("cost", 0.0);
        auto target_cat = static_cast<core::ProductCategory>(j.value("target_cat", 0));
        auto min_tier = static_cast<core::UserTier>(j.value("min_tier", 0));
        auto time_limit = static_cast<core::TimeConstraint>(j.value("time_limit", 0));
        std::string instance_id = j.value("instance_id", "");

        auto coupon = std::make_unique<ThresholdCoupon>(
            std::move(template_id),
            std::move(name),
            cost,
            min_spend,
            target_cat,
            min_tier,
            time_limit,
            reduce_amount,
            std::move(instance_id)
        );

        if (j.value("is_used", false))
        {
            coupon->markAsUsed();
        }
        return coupon;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
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
    try
    {
        if (j.value("type", "") != "DISCOUNT")
        {
            return nullptr;
        }

        std::string template_id = j.at("template_id").get<std::string>();
        std::string name = j.at("name").get<std::string>();
        double discount_rate = j.at("discount_rate").get<double>();

        double cost = j.value("cost", 0.0);
        double min_spend = j.value("min_spend", 0.0);
        double max_discount = j.value("max_discount", 0.0);
        auto target_cat = static_cast<core::ProductCategory>(j.value("target_cat", 0));
        auto min_tier = static_cast<core::UserTier>(j.value("min_tier", 0));
        auto time_limit = static_cast<core::TimeConstraint>(j.value("time_limit", 0));
        std::string instance_id = j.value("instance_id", "");

        auto coupon = std::make_unique<DiscountCoupon>(
            std::move(template_id),
            std::move(name),
            cost,
            min_spend,
            discount_rate,
            target_cat,
            min_tier,
            time_limit,
            max_discount,
            std::move(instance_id)
        );

        if (j.value("is_used", false))
        {
            coupon->markAsUsed();
        }
        return coupon;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
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
    try
    {
        if (j.value("type", "") != "CASH")
        {
            return nullptr;
        }

        std::string template_id = j.at("template_id").get<std::string>();
        std::string name = j.at("name").get<std::string>();
        double cash_amount = j.at("cash_amount").get<double>();

        double cost = j.value("cost", 0.0);
        auto target_cat = static_cast<core::ProductCategory>(j.value("target_cat", 0));
        auto min_tier = static_cast<core::UserTier>(j.value("min_tier", 0));
        auto time_limit = static_cast<core::TimeConstraint>(j.value("time_limit", 0));
        std::string instance_id = j.value("instance_id", "");

        auto coupon = std::make_unique<CashCoupon>(
            std::move(template_id),
            std::move(name),
            cost,
            cash_amount,
            target_cat,
            min_tier,
            time_limit,
            std::move(instance_id)
        );

        if (j.value("is_used", false))
        {
            coupon->markAsUsed();
        }
        return coupon;
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}

} // namespace shop