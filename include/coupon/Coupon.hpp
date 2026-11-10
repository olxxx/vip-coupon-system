#pragma once
#include "common/Types.hpp"
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace shop
{

// 优惠券抽象基类 (面向对象多态核心)
class Coupon
{
protected:
    std::string template_id;           // 券模板唯一编号 (如 "TPL_WEEKEND_20")
    std::string instance_id;           // 背包中的实例唯一编号 (如 "INST_20261028_0001", 模板中为空)
    std::string name;                  // 券面显示名称 (如 "周末数码特惠满减券")
    double cost;                       // 购券费用 (0.0为免费领取, >0.0为付费购买)
    double min_spend;                  // 满减使用门槛 (元)
    core::ProductCategory target_cat;  // 限制适用品类
    core::UserTier min_tier;           // 最低可用会员等级
    core::TimeConstraint time_limit;   // 时间生效约束
    bool is_used=false;                // 是否已被消费核销

    nlohmann::json toJsonBase() const
    {
        return nlohmann::json{
                {"template_id", template_id},
                {"instance_id", instance_id},
                {"name", name},
                {"cost", cost},
                {"min_spend", min_spend},
                {"target_cat", static_cast<int>(target_cat)},
                {"min_tier", static_cast<int>(min_tier)},
                {"time_limit", static_cast<int>(time_limit)},
                {"is_used", is_used}
            };
    }

public:
    Coupon(std::string template_id,std::string name,double cost,double min_spend,
           core::ProductCategory target_cat,core::UserTier min_tier,
           core::TimeConstraint time_limit,std::string instance_id="")
        : template_id(std::move(template_id)),instance_id(std::move(instance_id)),
          name(std::move(name)),cost(cost),min_spend(min_spend),
          target_cat(target_cat),min_tier(min_tier),time_limit(time_limit)
    {
    }

    virtual ~Coupon()=default;

    // --- 只读属性访问 ---
    const std::string& getTemplateId() const { return template_id; }
    const std::string& getInstanceId() const { return instance_id; }
    void setInstanceId(std::string new_instance_id) { instance_id=std::move(new_instance_id); }
    const std::string& getName() const { return name; }
    double getCost() const { return cost; }
    double getMinSpend() const { return min_spend; }
    core::ProductCategory getTargetCategory() const { return target_cat; }
    core::UserTier getMinTier() const { return min_tier; }
    core::TimeConstraint getTimeConstraint() const { return time_limit; }
    bool isUsed() const { return is_used; }

    //快速查询
    bool isFree() const { return cost<=1e-4; }
    bool isTemplate() const { return instance_id.empty(); }
    bool isUniversalCategory() const { return target_cat==core::ProductCategory::ALL; }
    bool hasTimeConstraint() const { return time_limit!=core::TimeConstraint::ANYTIME; }

    // 核销状态管理
    void markAsUsed() { is_used=true; }
    void resetUsed() { is_used=false; }

    // --- 纯虚接口 ---
    virtual std::string getTypeDescription() const=0;
    virtual double calculateDiscount(double eligible_amount) const=0;
    virtual std::string getRuleDescription() const=0;
    virtual std::unique_ptr<Coupon> clone() const=0;
    virtual nlohmann::json toJson() const=0;

    //安全克隆实例
    std::unique_ptr<Coupon> cloneInstance(const std::string& new_instance_id) const
    {
        auto inst = clone();
        inst->setInstanceId(new_instance_id);
        inst->resetUsed();
        return inst;
    }
};

} // namespace shop
