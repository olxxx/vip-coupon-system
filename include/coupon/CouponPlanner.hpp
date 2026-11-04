#pragma once
#include "common/Types.hpp"
#include "coupon/Coupon.hpp"
#include "shop/Cart.hpp"
#include "user/CustomerAccount.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace shop
{

//券是否可用的返回结果
struct EligbilityCheck
{
    bool is_eligible=false;
    std::string fail_reason;
    double applicable_amount=0.0;
    double potential_discount=0.0;
    double shortfall_amount=0.0;
};

//规划引擎的返回结果
struct PlanResult
{
    std::shared_ptr<Coupon> best_coupon=nullptr;
    double origin_total=0.0;
    double member_discount_saving=0.0;
    double coupon_discount_saving=0.0;
    double final_pay_amount=0.0;
};

//排序后的返回结果
struct CouponEvaluationItem
{
    std::shared_ptr<Coupon> coupon=nullptr;
    EligbilityCheck check;
    bool is_recommended=false;
};

class CouponPlanner
{
public:
    CouponPlanner()=default;

    //工具函数:计算会员折扣量
    static double calculateMemberSaving(double original_total,core::UserTier user_tier);

    //判断券是否可用
    static EligbilityCheck evaluateEligbility
    (
        const Coupon& coupon,
        const Cart& cart,
        core::UserTier user_tier,
        core::DayOfWeek current_day
    );

    //优惠券规划引擎
    static PlanResult computeOptimalPlan
    (
           const Cart& cart,
           const std::vector<std::shared_ptr<Coupon>>& user_coupons,
           core::UserTier user_tier,
           core::DayOfWeek current_day
    );

    //用户手动选择优惠券
    static PlanResult computeManualPlan
    (
        const Cart& cart,
        std::shared_ptr<Coupon> selected_coupon,
        core::UserTier user_tier,
        core::DayOfWeek current_day
    );

    //按照可用性和折扣排序优惠券
    static std::vector<CouponEvaluationItem> evaluateAndSortAllCoupons
    (
        const Cart& cart,
        const std::vector<std::shared_ptr<Coupon>>& user_coupons,
        core::UserTier user_tier,
        core::DayOfWeek current_day
    );

};

}
