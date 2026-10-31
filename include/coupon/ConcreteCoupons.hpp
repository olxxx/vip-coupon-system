#pragma once
#include "coupon/Coupon.hpp"
#include <algorithm>
#include <utility>

namespace shop
{

class ThresholdCoupon : public Coupon
{
private:
    double reduce_amount;

public:
    ThresholdCoupon(std::string template_id, std::string name, double cost, double min_spend,
                    core::ProductCategory target_cat, core::UserTier min_tier,
                    core::TimeConstraint time_limit, double reduce_amount,
                    std::string instance_id = "")
        : Coupon(std::move(template_id), std::move(name), cost, min_spend, target_cat, min_tier, time_limit, std::move(instance_id)),
          reduce_amount(reduce_amount) {}

    double getReduceAmount() const { return reduce_amount; }

    // 虚函数覆盖
    std::string getTypeDescription() const override { return "满减券"; }
    double calculateDiscount(double eligible_amount) const override;
    std::string getRuleDescription() const override;
    std::unique_ptr<Coupon> clone() const override { return std::make_unique<ThresholdCoupon>(*this); }
    nlohmann::json toJson() const override;

    static std::unique_ptr<Coupon> fromJson(const nlohmann::json& j);
};

class DiscountCoupon : public Coupon
{
private:
    double discount_rate;
    double max_discount;

public:
    DiscountCoupon(std::string template_id, std::string name, double cost, double min_spend, double discount_rate,
                   core::ProductCategory target_cat, core::UserTier min_tier,
                   core::TimeConstraint time_limit, double max_discount,
                   std::string instance_id = "")
        : Coupon(std::move(template_id), std::move(name), cost, min_spend, target_cat, min_tier, time_limit, std::move(instance_id)),
          discount_rate(discount_rate), max_discount(max_discount) {}

    double getDiscountRate() const { return discount_rate; }
    double getMaxDiscount() const { return max_discount; }
    bool hasMaxDiscount() const { return max_discount > 1e-4; }

    // 虚函数覆盖
    std::string getTypeDescription() const override { return "折扣券"; }
    double calculateDiscount(double eligible_amount) const override;
    std::string getRuleDescription() const override;
    std::unique_ptr<Coupon> clone() const override { return std::make_unique<DiscountCoupon>(*this); }
    nlohmann::json toJson() const override;

    static std::unique_ptr<Coupon> fromJson(const nlohmann::json& j);
};

class CashCoupon : public Coupon
{
private:
    double cash_amount;

public:
    CashCoupon(std::string template_id, std::string name, double cost, double cash_amount,
               core::ProductCategory target_cat, core::UserTier min_tier,
               core::TimeConstraint time_limit, std::string instance_id = "")
        : Coupon(std::move(template_id), std::move(name), cost, 0.0, target_cat, min_tier, time_limit, std::move(instance_id)),
          cash_amount(cash_amount) {}

    double getDiscountAmount() const { return cash_amount; }

    // 虚函数覆盖
    std::string getTypeDescription() const override { return "立减券"; }
    double calculateDiscount(double eligible_amount) const override;
    std::string getRuleDescription() const override;
    std::unique_ptr<Coupon> clone() const override { return std::make_unique<CashCoupon>(*this); }
    nlohmann::json toJson() const override;

    static std::unique_ptr<Coupon> fromJson(const nlohmann::json& j);
};

} // namespace shop
