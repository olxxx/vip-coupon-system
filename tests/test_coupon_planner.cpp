#include "coupon/CouponPlanner.hpp"
#include "coupon/ConcreteCoupons.hpp"
#include "shop/Cart.hpp"
#include "shop/Product.hpp"
#include "common/Types.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <memory>

inline bool isClose(double a, double b)
{
    return std::abs(a - b) < 1e-4;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] CouponPlanner 算法与推荐引擎测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 准备测试商品 (id, name, category, price, stock)
    auto phone = std::make_shared<shop::Product>("P001", "智能手机", core::ProductCategory::DIGITAL, 2000.0, 10);
    auto shirt = std::make_shared<shop::Product>("P002", "纯棉衬衫", core::ProductCategory::CLOTHING, 200.0, 20);
    auto snack = std::make_shared<shop::Product>("P003", "进口零食", core::ProductCategory::FOOD, 50.0, 50);

    // 1. calculateMemberSaving 会员让利算法测试
    {
        // V0 普通会员：0 折扣率 = 0 优惠
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V0_NORMAL), 0.0));
        // V1 青铜会员：98折，省 2% = 20.0
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V1_BRONZE), 20.0));
        // V2 白银会员：95折，省 5% = 50.0
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V2_SILVER), 50.0));
        // V3 黄金会员：90折，省 10% = 100.0
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V3_GOLD), 100.0));
        // V4 钻石会员：85折，省 15% = 150.0
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V4_DIAMOND), 150.0));
        // V5 至尊会员：80折，省 20% = 200.0
        assert(isClose(shop::CouponPlanner::calculateMemberSaving(1000.0, core::UserTier::V5_SUPREME), 200.0));

        std::cout << "[PASS] 1. calculateMemberSaving 各会员等级计算校验通过" << std::endl;
    }

    // 2. evaluateEligbility 四维约束诊断测试
    {
        shop::Cart cart;
        cart.addItem(shirt, 1); // 200 元服装
        cart.addItem(snack, 1); // 50 元食品，总额 250 元

        // A. 已使用券测试
        {
            shop::ThresholdCoupon cpn("T01", "满200减30", 0.0, 200.0, core::ProductCategory::ALL,
                                      core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0);
            cpn.markAsUsed();
            auto check = shop::CouponPlanner::evaluateEligbility(cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(!check.is_eligible);
            assert(check.fail_reason == "优惠券已被使用");
        }

        // B. 用户等级不足测试
        {
            shop::ThresholdCoupon cpn("T02", "白银专享券", 0.0, 100.0, core::ProductCategory::ALL,
                                      core::UserTier::V2_SILVER, core::TimeConstraint::ANYTIME, 20.0);
            auto check = shop::CouponPlanner::evaluateEligbility(cpn, cart, core::UserTier::V1_BRONZE, core::DayOfWeek::WEDNESDAY);
            assert(!check.is_eligible);
            assert(check.fail_reason == "用户等级不足");
        }

        // C. 时间约束测试（工作日 vs 周末）
        {
            shop::ThresholdCoupon weekday_cpn("T03", "工作日特惠", 0.0, 100.0, core::ProductCategory::ALL,
                                              core::UserTier::V0_NORMAL, core::TimeConstraint::WEEKDAY_ONLY, 20.0);
            // 周三可用
            auto check1 = shop::CouponPlanner::evaluateEligbility(weekday_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(check1.is_eligible);

            // 周六不可用
            auto check2 = shop::CouponPlanner::evaluateEligbility(weekday_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::SATURDAY);
            assert(!check2.is_eligible);
            assert(check2.fail_reason == "仅限工作日使用");

            shop::ThresholdCoupon weekend_cpn("T04", "周末狂欢", 0.0, 100.0, core::ProductCategory::ALL,
                                              core::UserTier::V0_NORMAL, core::TimeConstraint::WEEKEND_ONLY, 20.0);
            // 周三不可用
            auto check3 = shop::CouponPlanner::evaluateEligbility(weekend_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(!check3.is_eligible);
            assert(check3.fail_reason == "仅限周末使用");

            // 周日可用
            auto check4 = shop::CouponPlanner::evaluateEligbility(weekend_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::SUNDAY);
            assert(check4.is_eligible);
        }

        // D. 品类与金额门槛测试
        {
            // 数码类满 1000 减 100，当前购物车无任何数码商品
            shop::ThresholdCoupon digi_cpn("T05", "数码大额券", 0.0, 1000.0, core::ProductCategory::DIGITAL,
                                           core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 100.0);
            auto check = shop::CouponPlanner::evaluateEligbility(digi_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(!check.is_eligible);
            assert(check.fail_reason == "商品金额不足");
            assert(isClose(check.applicable_amount, 0.0));
            assert(isClose(check.shortfall_amount, 1000.0));

            // 服装类满 300 减 50，当前服装金额 200，差 100
            shop::ThresholdCoupon cloth_cpn("T06", "服装满300券", 0.0, 300.0, core::ProductCategory::CLOTHING,
                                            core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 50.0);
            auto check_cloth = shop::CouponPlanner::evaluateEligbility(cloth_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(!check_cloth.is_eligible);
            assert(check_cloth.fail_reason == "商品金额不足");
            assert(isClose(check_cloth.applicable_amount, 200.0));
            assert(isClose(check_cloth.shortfall_amount, 100.0));

            // 全品类满 200 减 30，当前总额 250，达标！
            shop::ThresholdCoupon all_cpn("T07", "全品类满200券", 0.0, 200.0, core::ProductCategory::ALL,
                                          core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0);
            auto check_all = shop::CouponPlanner::evaluateEligbility(all_cpn, cart, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
            assert(check_all.is_eligible);
            assert(isClose(check_all.applicable_amount, 250.0));
            assert(isClose(check_all.potential_discount, 30.0));
            assert(isClose(check_all.shortfall_amount, 0.0));
        }

        std::cout << "[PASS] 2. evaluateEligbility 四维约束与差额诊断测试通过" << std::endl;
    }

    // 3. computeOptimalPlan 单券最优规划与平行让利闭环测试
    {
        shop::Cart cart;
        cart.addItem(phone, 1); // 数码 2000 元
        cart.addItem(shirt, 1); // 服装 200 元，总额 2200 元

        // 黄金会员 V3：打 9 折，会员省 2200 * 0.10 = 220 元
        core::UserTier tier = core::UserTier::V3_GOLD;

        // 背包卡券库：
        // 券 1：满 200 减 30
        auto cpn1 = std::make_shared<shop::ThresholdCoupon>(
            "C01", "满200减30", 0.0, 200.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0);
        // 券 2：数码满 1000 享 85 折 (0.85)，能省 2000 * 0.15 = 300 元
        auto cpn2 = std::make_shared<shop::DiscountCoupon>(
            "C02", "数码85折券", 0.0, 1000.0, 0.85, core::ProductCategory::DIGITAL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 0.0);
        // 券 3：仅周末可用的满 1000 减 500
        auto cpn3 = std::make_shared<shop::ThresholdCoupon>(
            "C03", "周末满1000减500", 0.0, 1000.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::WEEKEND_ONLY, 500.0);

        std::vector<std::shared_ptr<shop::Coupon>> user_coupons = {cpn1, cpn2, cpn3};

        // 场景 A：周三工作日，cpn3 不可用，系统应选 cpn2 (省 300 > cpn1 的 30)
        auto plan_wed = shop::CouponPlanner::computeOptimalPlan(cart, user_coupons, tier, core::DayOfWeek::WEDNESDAY);
        assert(plan_wed.best_coupon == cpn2);
        assert(isClose(plan_wed.origin_total, 2200.0));
        assert(isClose(plan_wed.member_discount_saving, 220.0)); // 会员省 220
        assert(isClose(plan_wed.coupon_discount_saving, 300.0)); // 券省 300
        // 实付 = 2200 - 220 - 300 = 1680
        assert(isClose(plan_wed.final_pay_amount, 1680.0));

        // 场景 B：周六，cpn3 可用，系统应选省更多的 cpn3 (省 500 > cpn2 的 300)
        auto plan_sat = shop::CouponPlanner::computeOptimalPlan(cart, user_coupons, tier, core::DayOfWeek::SATURDAY);
        assert(plan_sat.best_coupon == cpn3);
        assert(isClose(plan_sat.coupon_discount_saving, 500.0));
        // 实付 = 2200 - 220 - 500 = 1480
        assert(isClose(plan_sat.final_pay_amount, 1480.0));

        // 场景 C：用户背包无可用券
        std::vector<std::shared_ptr<shop::Coupon>> empty_coupons;
        auto plan_none = shop::CouponPlanner::computeOptimalPlan(cart, empty_coupons, tier, core::DayOfWeek::WEDNESDAY);
        assert(plan_none.best_coupon == nullptr);
        assert(isClose(plan_none.coupon_discount_saving, 0.0));
        assert(isClose(plan_none.final_pay_amount, 2200.0 - 220.0)); // 仅会员折后价 1980

        std::cout << "[PASS] 3. computeOptimalPlan 最优推荐与平行让利计算通过" << std::endl;
    }

    // 4. computeManualPlan 手动选券与降级测试
    {
        shop::Cart cart;
        cart.addItem(shirt, 1); // 200 元服装
        core::UserTier tier = core::UserTier::V1_BRONZE; // 98 折，省 4 元

        auto cpn_valid = std::make_shared<shop::ThresholdCoupon>(
            "C01", "满200减30", 0.0, 200.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0);
        auto cpn_invalid = std::make_shared<shop::ThresholdCoupon>(
            "C02", "满500减100", 0.0, 500.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 100.0);

        // A. 传入 nullptr（用户勾选不使用券）
        auto plan_null = shop::CouponPlanner::computeManualPlan(cart, nullptr, tier, core::DayOfWeek::WEDNESDAY);
        assert(plan_null.best_coupon == nullptr);
        assert(isClose(plan_null.origin_total, 200.0));
        assert(isClose(plan_null.member_discount_saving, 4.0));
        assert(isClose(plan_null.coupon_discount_saving, 0.0));
        assert(isClose(plan_null.final_pay_amount, 196.0));

        // B. 传入合规券
        auto plan_valid = shop::CouponPlanner::computeManualPlan(cart, cpn_valid, tier, core::DayOfWeek::WEDNESDAY);
        assert(plan_valid.best_coupon == cpn_valid);
        assert(isClose(plan_valid.coupon_discount_saving, 30.0));
        assert(isClose(plan_valid.final_pay_amount, 166.0)); // 200 - 4 - 30

        // C. 传入不合规券（应降级为不使用券）
        auto plan_invalid = shop::CouponPlanner::computeManualPlan(cart, cpn_invalid, tier, core::DayOfWeek::WEDNESDAY);
        assert(plan_invalid.best_coupon == nullptr);
        assert(isClose(plan_invalid.coupon_discount_saving, 0.0));
        assert(isClose(plan_invalid.final_pay_amount, 196.0));

        std::cout << "[PASS] 4. computeManualPlan 手动选券与优雅降级测试通过" << std::endl;
    }

    // 5. evaluateAndSortAllCoupons 批量分类排序测试
    {
        shop::Cart cart;
        cart.addItem(shirt, 1); // 200 元服装

        // 可用券 1：立减 10 元
        auto c1 = std::make_shared<shop::CashCoupon>(
            "C1", "10元立减", 0.0, 10.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME);
        // 可用券 2：满 200 减 30 元（更优）
        auto c2 = std::make_shared<shop::ThresholdCoupon>(
            "C2", "满200减30", 0.0, 200.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0);
        // 不可用券 1：满 250 减 50（差 50 元）
        auto c3 = std::make_shared<shop::ThresholdCoupon>(
            "C3", "满250减50", 0.0, 250.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 50.0);
        // 不可用券 2：满 300 减 80（差 100 元）
        auto c4 = std::make_shared<shop::ThresholdCoupon>(
            "C4", "满300减80", 0.0, 300.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 80.0);
        // 不可用券 3：限周末使用（shortfall = 0，但时间不符）
        auto c5 = std::make_shared<shop::ThresholdCoupon>(
            "C5", "周末专享", 0.0, 100.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::WEEKEND_ONLY, 20.0);

        std::vector<std::shared_ptr<shop::Coupon>> user_coupons = {c1, c5, c4, c2, c3};

        auto sorted = shop::CouponPlanner::evaluateAndSortAllCoupons(cart, user_coupons, core::UserTier::V0_NORMAL, core::DayOfWeek::WEDNESDAY);
        assert(sorted.size() == 5);

        // 前两个必须是可用券：c2（推荐，减30），然后是 c1（减10）
        assert(sorted[0].check.is_eligible);
        assert(sorted[0].coupon == c2);
        assert(sorted[0].is_recommended);

        assert(sorted[1].check.is_eligible);
        assert(sorted[1].coupon == c1);
        assert(!sorted[1].is_recommended);

        // 后三个必须是不可用券：
        // 且金额差额小的排在前面（c3 差50 排在 c4 差100 前面，c5 无差额排在最后）
        assert(!sorted[2].check.is_eligible);
        assert(sorted[2].coupon == c3);
        assert(isClose(sorted[2].check.shortfall_amount, 50.0));

        assert(!sorted[3].check.is_eligible);
        assert(sorted[3].coupon == c4);
        assert(isClose(sorted[3].check.shortfall_amount, 100.0));

        assert(!sorted[4].check.is_eligible);
        assert(sorted[4].coupon == c5);

        std::cout << "[PASS] 5. evaluateAndSortAllCoupons 批量分类与智能排序测试通过" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "全部 CouponPlanner 单元测试 100% 通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
