#include "coupon/ConcreteCoupons.hpp"
#include "common/Types.hpp"
#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <memory>

// 辅助浮点断言
inline bool isClose(double a, double b)
{
    return std::abs(a - b) < 1e-4;
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] 优惠券派生类体系 自动化测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 基类通用属性与语义判断方法测试
    {
        // ThresholdCoupon 构造顺序: template_id, name, cost, min_spend, target_cat, min_tier, time_limit, reduce_amount
        shop::ThresholdCoupon tpl(
            "TPL_DIGITAL_100", "数码特惠满减券", 0.0, 500.0,
            core::ProductCategory::DIGITAL, core::UserTier::V2_SILVER,
            core::TimeConstraint::WEEKEND_ONLY, 100.0
        );

        assert(tpl.getTemplateId() == "TPL_DIGITAL_100");
        assert(tpl.getName() == "数码特惠满减券");
        assert(tpl.isFree());
        assert(tpl.isTemplate()); // 尚未绑定实例ID
        assert(!tpl.isUniversalCategory()); // 限数码3C
        assert(tpl.hasTimeConstraint()); // 限周末
        assert(!tpl.isUsed());

        // 测试安全克隆与实例派生
        tpl.markAsUsed();
        assert(tpl.isUsed());

        auto inst = tpl.cloneInstance("INST_20261031_0001");
        assert(inst != nullptr);
        assert(!inst->isTemplate());
        assert(inst->getInstanceId() == "INST_20261031_0001");
        assert(!inst->isUsed()); // 克隆实例必须自动复位为未核销状态
        assert(tpl.isUsed());    // 原始模板状态不应受影响

        std::cout << "[PASS] 1. 基类通用属性、语义查询与安全克隆校验" << std::endl;
    }

    // 2. 满减券 (ThresholdCoupon) 业务计算与规则测试
    {
        // 满 200 减 30，限全品类，付费购券 5 元
        shop::ThresholdCoupon tc(
            "TPL_TC_01", "日常满减券", 5.0, 200.0,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME, 30.0
        );

        assert(tc.getTypeDescription() == "满减券");
        assert(isClose(tc.getReduceAmount(), 30.0));
        assert(isClose(tc.getMinSpend(), 200.0));
        assert(!tc.isFree());
        assert(tc.isUniversalCategory());
        assert(!tc.hasTimeConstraint());

        // 优惠额度计算测试
        // A. 未达到门槛
        assert(isClose(tc.calculateDiscount(150.0), 0.0));
        assert(isClose(tc.calculateDiscount(199.99), 0.0));

        // B. 达到门槛并正常减免
        assert(isClose(tc.calculateDiscount(200.0), 30.0));
        assert(isClose(tc.calculateDiscount(500.0), 30.0));

        // C. 极端边界：若减免额大于商品实价 (如门槛 20 减 30，商品 25 元)，应封顶抵扣至 0 元防负数
        shop::ThresholdCoupon tc_edge(
            "TPL_TC_EDGE", "特殊大额券", 0.0, 20.0,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME, 30.0
        );
        assert(isClose(tc_edge.calculateDiscount(25.0), 25.0));

        // 规则描述校验
        std::string rule = tc.getRuleDescription();
        assert(rule.find("200.00") != std::string::npos);
        assert(rule.find("30.00") != std::string::npos);

        // JSON 序列化与反序列化往返测试
        nlohmann::json j = tc.toJson();
        assert(j["type"] == "THRESHOLD");
        assert(isClose(j["reduce_amount"], 30.0));
        assert(isClose(j["min_spend"], 200.0));

        auto restored = shop::ThresholdCoupon::fromJson(j);
        assert(restored != nullptr);
        assert(restored->getTemplateId() == tc.getTemplateId());
        assert(restored->getTypeDescription() == "满减券");
        assert(isClose(restored->calculateDiscount(300.0), 30.0));

        std::cout << "[PASS] 2. 满减券 (ThresholdCoupon) 计算、规则与 JSON 校验" << std::endl;
    }

    // 3. 折扣券 (DiscountCoupon) 业务计算与规则测试
    {
        // DiscountCoupon 构造顺序: template_id, name, cost, min_spend, discount_rate, target_cat, min_tier, time_limit, max_discount
        // 满 300 享 8 折 (0.80)，封顶 100 元
        shop::DiscountCoupon dc(
            "TPL_DC_01", "大额数码折扣券", 0.0, 300.0, 0.80,
            core::ProductCategory::DIGITAL, core::UserTier::V1_BRONZE,
            core::TimeConstraint::WEEKDAY_ONLY, 100.0
        );

        assert(dc.getTypeDescription() == "折扣券");
        assert(isClose(dc.getDiscountRate(), 0.80));
        assert(isClose(dc.getMaxDiscount(), 100.0));
        assert(dc.hasMaxDiscount());

        // A. 未达到满 300 门槛
        assert(isClose(dc.calculateDiscount(250.0), 0.0));

        // B. 达标且未达封顶 (400 元打 8 折 -> 减免 400 * 0.2 = 80 元)
        assert(isClose(dc.calculateDiscount(400.0), 80.0));

        // C. 达标且触发封顶 (1000 元打 8 折 -> 理论减 200 元，封顶 100 元)
        assert(isClose(dc.calculateDiscount(1000.0), 100.0));

        // D. 无封顶折扣券测试 (max_discount = 0.0)
        shop::DiscountCoupon dc_unlimited(
            "TPL_DC_UNLIMITED", "无封顶折扣券", 0.0, 200.0, 0.85,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME, 0.0
        );
        assert(!dc_unlimited.hasMaxDiscount());
        // 1000 元享 85 折 -> 减免 1000 * 0.15 = 150 元，无封顶
        assert(isClose(dc_unlimited.calculateDiscount(1000.0), 150.0));

        // JSON 往返测试
        nlohmann::json j = dc.toJson();
        assert(j["type"] == "DISCOUNT");
        assert(isClose(j["discount_rate"], 0.80));
        assert(isClose(j["max_discount"], 100.0));

        auto restored = shop::DiscountCoupon::fromJson(j);
        assert(restored != nullptr);
        assert(restored->getTypeDescription() == "折扣券");
        assert(isClose(restored->calculateDiscount(400.0), 80.0));

        std::cout << "[PASS] 3. 折扣券 (DiscountCoupon) 折扣算力、封顶保护与 JSON 校验" << std::endl;
    }

    // 4. 立减券 (CashCoupon) 业务计算与规则测试
    {
        // 5 元无门槛直减券
        // CashCoupon 构造顺序: template_id, name, cost, cash_amount, target_cat, min_tier, time_limit
        shop::CashCoupon cc(
            "TPL_CC_01", "新人5元立减券", 0.0, 5.0,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME
        );

        assert(cc.getTypeDescription() == "立减券");
        assert(isClose(cc.getDiscountAmount(), 5.0));
        assert(isClose(cc.getMinSpend(), 0.0));

        // A. 正常直减 (100 元立减 5 元 -> 减免 5 元)
        assert(isClose(cc.calculateDiscount(100.0), 5.0));

        // B. 极限免单 (3 元商品使用 5 元券 -> 最多减免 3 元，防倒贴为负)
        assert(isClose(cc.calculateDiscount(3.0), 3.0));

        // JSON 往返测试
        nlohmann::json j = cc.toJson();
        assert(j["type"] == "CASH");
        assert(isClose(j["cash_amount"], 5.0));

        auto restored = shop::CashCoupon::fromJson(j);
        assert(restored != nullptr);
        assert(restored->getTypeDescription() == "立减券");
        assert(isClose(restored->calculateDiscount(50.0), 5.0));

        std::cout << "[PASS] 4. 立减券 (CashCoupon) 无门槛减免与免单边界校验" << std::endl;
    }

    // 5. 多态动态绑定与混合容器多态行为测试
    {
        std::vector<std::unique_ptr<shop::Coupon>> pool;
        pool.push_back(std::make_unique<shop::ThresholdCoupon>("T1", "满200减30", 0.0, 200.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 30.0));
        pool.push_back(std::make_unique<shop::DiscountCoupon>("T2", "满300打8折", 0.0, 300.0, 0.80, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME, 50.0));
        pool.push_back(std::make_unique<shop::CashCoupon>("T3", "立减10元", 0.0, 10.0, core::ProductCategory::ALL, core::UserTier::V0_NORMAL, core::TimeConstraint::ANYTIME));

        // 统一通过基类指针调用虚函数，验证多态分发
        assert(pool[0]->getTypeDescription() == "满减券");
        assert(pool[1]->getTypeDescription() == "折扣券");
        assert(pool[2]->getTypeDescription() == "立减券");

        // 针对 400 元订单测试多态减免计算
        double test_order = 400.0;
        assert(isClose(pool[0]->calculateDiscount(test_order), 30.0)); // 满减：30
        assert(isClose(pool[1]->calculateDiscount(test_order), 50.0)); // 打折：400*0.2=80，封顶 50
        assert(isClose(pool[2]->calculateDiscount(test_order), 10.0)); // 立减：10

        // 验证多态原型 clone
        auto cloned = pool[1]->clone();
        assert(cloned != nullptr);
        assert(cloned->getTypeDescription() == "折扣券");
        assert(isClose(cloned->calculateDiscount(test_order), 50.0));

        // 验证多态序列化
        for (const auto& item : pool) {
            nlohmann::json item_j = item->toJson();
            assert(!item_j["type"].get<std::string>().empty());
            assert(!item_j["name"].get<std::string>().empty());
        }

        std::cout << "[PASS] 5. 异构容器多态派发与原型克隆校验" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "All ConcreteCoupons tests PASSED! 100%" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
