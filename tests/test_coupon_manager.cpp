#include "coupon/CouponManager.hpp"
#include "coupon/ConcreteCoupons.hpp"
#include "user/CustomerAccount.hpp"
#include "common/Types.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <memory>

inline bool isClose(double a, double b)
{
    return std::abs(a - b) < 1e-4;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] CouponManager 券中心服务层全套测试" << std::endl;
    std::cout << "========================================" << std::endl;

    shop::CouponManager cpn_mgr;

    // 1. 规模与状态初始验证
    {
        assert(cpn_mgr.empty());
        assert(cpn_mgr.size() == 0);
        assert(!cpn_mgr.hasCoupon("TPL_FREE_10"));
        std::cout << "[PASS] 1. 初始空池状态验证通过" << std::endl;
    }

    // 2. 优惠券模板上架与下架
    auto free_coupon = std::make_shared<shop::ThresholdCoupon>(
        "TPL_FREE_10", "新手免费满减券", 0.0, 100.0,
        core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
        core::TimeConstraint::ANYTIME, 10.0
    );

    auto paid_coupon = std::make_shared<shop::DiscountCoupon>(
        "TPL_PAID_DIGITAL", "数码超值9折券", 20.0, 500.0, 0.90,
        core::ProductCategory::DIGITAL, core::UserTier::V1_BRONZE,
        core::TimeConstraint::WEEKEND_ONLY, 100.0
    );

    auto cash_coupon = std::make_shared<shop::CashCoupon>(
        "TPL_CASH_VIP", "尊享会员立减券", 50.0, 60.0,
        core::ProductCategory::ALL, core::UserTier::V3_GOLD,
        core::TimeConstraint::ANYTIME
    );

    {
        assert(cpn_mgr.registerCouponToPool(free_coupon));
        assert(cpn_mgr.registerCouponToPool(paid_coupon));
        assert(cpn_mgr.registerCouponToPool(cash_coupon));
        assert(cpn_mgr.size() == 3);
        assert(!cpn_mgr.empty());
        assert(cpn_mgr.hasCoupon("TPL_FREE_10"));
        assert(cpn_mgr.hasCoupon("TPL_PAID_DIGITAL"));
        assert(cpn_mgr.findCouponInPool("TPL_FREE_10") != nullptr);
        assert(cpn_mgr.findCouponInPool("TPL_NOT_EXIST") == nullptr);

        // 重复注册同 ID 应覆盖
        auto free_coupon_updated = std::make_shared<shop::ThresholdCoupon>(
            "TPL_FREE_10", "新手免费满减券(升级版)", 0.0, 100.0,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME, 15.0
        );
        assert(cpn_mgr.registerCouponToPool(free_coupon_updated));
        assert(cpn_mgr.size() == 3);
        assert(cpn_mgr.findCouponInPool("TPL_FREE_10")->getName() == "新手免费满减券(升级版)");

        std::cout << "[PASS] 2. 券模板注册与同ID覆盖测试通过" << std::endl;
    }

    // 3. 视图检索与分类筛选测试
    {
        auto free_list = cpn_mgr.getFreeCoupons();
        assert(free_list.size() == 1);
        assert(free_list[0]->getTemplateId() == "TPL_FREE_10");

        auto paid_list = cpn_mgr.getPaidCoupons();
        assert(paid_list.size() == 2);

        auto digital_list = cpn_mgr.getCouponsByCategory(core::ProductCategory::DIGITAL);
        assert(digital_list.size() == 1);
        assert(digital_list[0]->getTemplateId() == "TPL_PAID_DIGITAL");

        auto all_cat_list = cpn_mgr.getCouponsByCategory(core::ProductCategory::ALL);
        assert(all_cat_list.size() == 3); // ALL 返回全部

        auto weekend_list = cpn_mgr.getCouponsByTimeConstraint(core::TimeConstraint::WEEKEND_ONLY);
        assert(weekend_list.size() == 1);

        user::CustomerAccount normal_user("alice", "pass", "2026-11-05", 100.0, 0.0, core::UserTier::V0_NORMAL);
        auto eligible_v0 = cpn_mgr.getEligibleCouponsForUser(normal_user);
        assert(eligible_v0.size() == 1); // 只有免费券要求 V0

        user::CustomerAccount gold_user("bob", "pass", "2026-11-05", 1000.0, 0.0, core::UserTier::V3_GOLD);
        auto eligible_v3 = cpn_mgr.getEligibleCouponsForUser(gold_user);
        assert(eligible_v3.size() == 3); // V3 可领全部三张

        std::cout << "[PASS] 3. 各种分类、品类与会员资格视图筛选通过" << std::endl;
    }

    // 4. 免费领券事务编排 (claimFreeCoupon)
    {
        user::CustomerAccount normal_user("alice", "pass", "2026-11-05", 100.0);
        
        // 领不存在的券
        auto res1 = cpn_mgr.claimFreeCoupon("TPL_NOT_EXIST", normal_user);
        assert(!res1.success);

        // 尝试免费领付费券
        auto res2 = cpn_mgr.claimFreeCoupon("TPL_PAID_DIGITAL", normal_user);
        assert(!res2.success);

        // 成功免费申领
        assert(normal_user.getCouponCount() == 0);
        auto res3 = cpn_mgr.claimFreeCoupon("TPL_FREE_10", normal_user);
        assert(res3.success);
        assert(normal_user.getCouponCount() == 1);

        auto cpn = normal_user.getCoupons()[0];
        assert(!cpn->getInstanceId().empty());
        assert(cpn->getInstanceId() == "INST_1001");
        assert(!cpn->isUsed());

        assert(cpn_mgr.hasUserHeldCoupon(normal_user, "TPL_FREE_10"));
        // 重复领取同模板免费券应被拦截
        auto res_dup = cpn_mgr.claimFreeCoupon("TPL_FREE_10", normal_user);
        assert(!res_dup.success);
        assert(normal_user.getCouponCount() == 1);

        // 一键申领全部免费券 (此时用户已拥有，应返回无新券可领)
        auto res_all_none = cpn_mgr.claimAllFreeCoupons(normal_user);
        assert(!res_all_none.success);

        // 新用户一键申领测试
        // 注意：此处会消耗 INST_1002，后续第 5、7 节的硬编码序号依赖本用例的执行顺序；
        // 若在此处之前新增任何成功领券/购券用例，需同步更新后续所有 INST_xxxx 断言。
        user::CustomerAccount fresh_user("david", "pass", "2026-11-05", 100.0);
        auto res_all_ok = cpn_mgr.claimAllFreeCoupons(fresh_user);
        assert(res_all_ok.success);
        assert(fresh_user.getCouponCount() == 1);

        std::cout << "[PASS] 4. 免费领券事务与实例自增测试通过" << std::endl;
    }

    // 5. 付费购券事务编排 (buyPaidCoupon)
    {
        // 创建一个 V0 普通用户，余额 500
        user::CustomerAccount normal_user("alice", "pass", "2026-11-05", 500.0, 0.0, core::UserTier::V0_NORMAL);

        // 1. 等级不够购买付费券 (TPL_PAID_DIGITAL 需要 V1)
        auto res_tier = cpn_mgr.buyPaidCoupon("TPL_PAID_DIGITAL", normal_user);
        assert(!res_tier.success);

        // 2. 将用户升级到 V1 青铜会员，但余额仅 10 元 (券需 20 元)
        user::CustomerAccount poor_vip("charlie", "pass", "2026-11-05", 10.0, 200.0, core::UserTier::V1_BRONZE);
        auto res_poor = cpn_mgr.buyPaidCoupon("TPL_PAID_DIGITAL", poor_vip);
        assert(!res_poor.success); // 余额不足
        assert(poor_vip.getCouponCount() == 0);
        assert(isClose(poor_vip.getBalance(), 10.0));

        // 3. 正常购买
        poor_vip.recharge(100.0); // 余额变为 110
        auto res_buy = cpn_mgr.buyPaidCoupon("TPL_PAID_DIGITAL", poor_vip);
        assert(res_buy.success);
        assert(poor_vip.getCouponCount() == 1);
        assert(isClose(poor_vip.getBalance(), 90.0)); // 110 - 20 = 90
        assert(poor_vip.getCoupons()[0]->getInstanceId() == "INST_1003");
        assert(poor_vip.getTransactionCount() >= 2); // 充值 + 购券流水

        assert(cpn_mgr.hasUserHeldCoupon(poor_vip, "TPL_PAID_DIGITAL"));
        // 重复购买同模板付费券应被拦截
        auto res_dup_paid = cpn_mgr.buyPaidCoupon("TPL_PAID_DIGITAL", poor_vip);
        assert(!res_dup_paid.success);
        assert(poor_vip.getCouponCount() == 1);

        std::cout << "[PASS] 5. 付费购券扣款、等级门槛与流水生成测试通过" << std::endl;
    }

    // 6. 下架功能
    {
        assert(cpn_mgr.removeCouponFromPool("TPL_FREE_10"));
        assert(cpn_mgr.size() == 2);
        assert(!cpn_mgr.hasCoupon("TPL_FREE_10"));
        assert(!cpn_mgr.removeCouponFromPool("TPL_FREE_10")); // 再次删除返回 false
        std::cout << "[PASS] 6. 下架功能测试通过" << std::endl;
    }

    // 7. JSON 序列化与反序列化
    {
        nlohmann::json j = cpn_mgr.toJson();
        assert(j.contains("instance_counter"));
        assert(j.contains("public_pool"));
        assert(j["instance_counter"] == 1003);

        shop::CouponManager restored = shop::CouponManager::fromJson(j);
        assert(restored.size() == 2);
        assert(restored.getInstanceCounter() == 1003);
        assert(restored.hasCoupon("TPL_PAID_DIGITAL"));
        assert(restored.hasCoupon("TPL_CASH_VIP"));

        // 新生成的券实例 ID 应该紧随其后
        assert(restored.generateNextInstanceId() == "INST_1004");

        std::cout << "[PASS] 7. JSON 序列化与反序列化及实例序号恢复测试通过" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有 CouponManager 单元测试 100% 通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
