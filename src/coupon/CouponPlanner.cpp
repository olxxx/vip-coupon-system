#include "coupon/CouponPlanner.hpp"
#include "common/Types.hpp"
#include <algorithm>

namespace shop
{

double CouponPlanner::calculateMemberSaving(double original_total, core::UserTier user_tier)
{
    return original_total*(1.0-getTierConfig(user_tier).discount_rate);
}

EligbilityCheck CouponPlanner::evaluateEligbility(
    const Coupon& coupon,
    const Cart& cart,
    core::UserTier user_tier,
    core::DayOfWeek current_day)
{
    EligbilityCheck result;
    result.is_eligible=false;
    if(coupon.isUsed())
    {
        result.fail_reason = "优惠券已被使用";
        return result;
    }
    if(coupon.getMinTier()>user_tier)
    {
        result.fail_reason = "用户等级不足";
        return result;
    }
    if(coupon.hasTimeConstraint() && coupon.getTimeConstraint()!=core::TimeConstraint::ANYTIME)
    {
        if(core::isWeekend(current_day)&&coupon.getTimeConstraint()==core::TimeConstraint::WEEKDAY_ONLY)
        {
            result.fail_reason = "仅限工作日使用";
            return result;
        }
        if(!core::isWeekend(current_day)&&coupon.getTimeConstraint()==core::TimeConstraint::WEEKEND_ONLY)
        {
            result.fail_reason = "仅限周末使用";
            return result;
        }
    }
    double category_amount=cart.getCategoryAmount(coupon.getTargetCategory());
    if(!core::isAmountGTE(category_amount, coupon.getMinSpend()))
    {
        result.fail_reason = "商品金额不足";
        result.applicable_amount = category_amount;
        result.shortfall_amount = coupon.getMinSpend() - category_amount;
        return result;
    }
    result.applicable_amount = category_amount;
    result.potential_discount = coupon.calculateDiscount(category_amount);
    result.is_eligible=true;
    return result;
}

PlanResult CouponPlanner::computeOptimalPlan(
    const Cart& cart,
    const std::vector<std::shared_ptr<Coupon>>& user_coupons,
    core::UserTier user_tier,
    core::DayOfWeek current_day)
{
    PlanResult result;
    result.origin_total = cart.getTotalOriginalAmount();
    result.member_discount_saving = CouponPlanner::calculateMemberSaving(result.origin_total, user_tier);
    for(const auto& coupon:user_coupons)
    {
        if(!coupon) continue;
        EligbilityCheck check = evaluateEligbility(*coupon, cart, user_tier, current_day);
        if(check.is_eligible&&check.potential_discount>result.coupon_discount_saving)
        {
            result.coupon_discount_saving = check.potential_discount;
            result.best_coupon = coupon;
        }
    }
    result.coupon_discount_saving = std::min(result.origin_total-result.member_discount_saving, result.coupon_discount_saving);
    result.final_pay_amount = std::max(0.0,result.origin_total - result.member_discount_saving - result.coupon_discount_saving);
    return result;
}


PlanResult CouponPlanner::computeManualPlan(
    const Cart& cart,
    std::shared_ptr<Coupon> selected_coupon,
    core::UserTier user_tier,
    core::DayOfWeek current_day)
{
    PlanResult result;
    result.origin_total = cart.getTotalOriginalAmount();
    result.member_discount_saving = CouponPlanner::calculateMemberSaving(result.origin_total, user_tier);
    if(!selected_coupon)
    {
        result.final_pay_amount = result.origin_total - result.member_discount_saving;
        return result;
    }
    EligbilityCheck check = evaluateEligbility(*selected_coupon, cart, user_tier, current_day);
    if(check.is_eligible)
    {
        result.best_coupon = selected_coupon;
        result.coupon_discount_saving = std::min(check.potential_discount, result.origin_total - result.member_discount_saving);
    }
    result.final_pay_amount = std::max(0.0,result.origin_total - result.member_discount_saving - result.coupon_discount_saving);
    return result;
}


std::vector<CouponEvaluationItem> CouponPlanner::evaluateAndSortAllCoupons(
    const Cart& cart,
    const std::vector<std::shared_ptr<Coupon>>& user_coupons,
    core::UserTier user_tier,
    core::DayOfWeek current_day)
{
    PlanResult optimal = computeOptimalPlan(cart, user_coupons, user_tier, current_day);
    std::shared_ptr<Coupon> best_coupon = optimal.best_coupon;
    std::vector<CouponEvaluationItem> sorted_coupons;
    std::vector<CouponEvaluationItem> unavailable_coupons;
    for(const auto& coupon : user_coupons)
    {
        if(!coupon) continue;
        EligbilityCheck check = evaluateEligbility(*coupon, cart, user_tier, current_day);
        if(check.is_eligible)
            sorted_coupons.push_back({coupon, check, coupon == best_coupon});
        else
            unavailable_coupons.push_back({coupon, check, false});
    }
    std::sort(sorted_coupons.begin(), sorted_coupons.end(),
        [](const CouponEvaluationItem& a, const CouponEvaluationItem& b)
        {
            if (a.is_recommended != b.is_recommended)
                return a.is_recommended;
            return a.check.potential_discount > b.check.potential_discount;
        });
    std::sort(unavailable_coupons.begin(), unavailable_coupons.end(),
        [](const CouponEvaluationItem& a, const CouponEvaluationItem& b)
        {
            if(a.check.shortfall_amount>0&&b.check.shortfall_amount>0)
                return a.check.shortfall_amount < b.check.shortfall_amount;
            if(a.check.shortfall_amount>0) return true;
            if(b.check.shortfall_amount>0) return false;
            return false;
        });
    sorted_coupons.insert(sorted_coupons.end(), unavailable_coupons.begin(), unavailable_coupons.end());
    return sorted_coupons;
}

}
