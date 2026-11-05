#include "coupon/CouponManager.hpp"
#include "common/Result.hpp"
#include "persist/JsonFactories.hpp"

namespace shop
{

bool CouponManager::hasCoupon(const std::string &template_id) const
{
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getTemplateId() == template_id)
            return true;
    return false;
}

bool CouponManager::registerCouponToPool(std::shared_ptr<Coupon> coupon)
{
    if (!coupon)
        return false;

    for (size_t i = 0; i < public_pool.size(); ++i)
    {
        if (public_pool[i] && public_pool[i]->getTemplateId() == coupon->getTemplateId())
        {
            public_pool[i] = coupon; // 存在同ID模板时覆盖更新
            return true;
        }
    }
    public_pool.push_back(coupon);
    return true;
}

bool CouponManager::removeCouponFromPool(const std::string &template_id)
{
    for (auto it = public_pool.begin(); it != public_pool.end(); ++it)
    {
        if (*it && (*it)->getTemplateId() == template_id)
        {
            public_pool.erase(it);
            return true;
        }
    }
    return false;
}

core::Result CouponManager::claimFreeCoupon(const std::string &template_id, user::CustomerAccount &customer)
{
    auto coupon = findCouponInPool(template_id);
    if (!coupon)
        return core::Result::fail("未找到该优惠券");

    if (!coupon->isFree())
        return core::Result::fail("该优惠券为付费券，请前往购券中心购买");

    if (customer.getTier() < coupon->getMinTier())
        return core::Result::fail("会员等级未达到该优惠券的领取门槛");

    std::string instance_id = generateNextInstanceId();
    auto instance_coupon = coupon->cloneInstance(instance_id);
    customer.addCoupon(std::move(instance_coupon));
    return core::Result::ok("优惠券领取成功");
}

core::Result CouponManager::buyPaidCoupon(const std::string &template_id, user::CustomerAccount &customer)
{
    auto coupon = findCouponInPool(template_id);
    if (!coupon)
        return core::Result::fail("未找到该优惠券");

    if (coupon->isFree())
        return core::Result::fail("该优惠券为免费券，可直接申领");

    if (customer.getTier() < coupon->getMinTier())
        return core::Result::fail("会员等级未达到该优惠券的购买门槛");

    // 扣减余额并记录流水
    if (!customer.deductForCoupon(coupon->getCost(), coupon->getName(), coupon->getTemplateId()))
        return core::Result::fail("账户余额不足，购券失败");

    std::string instance_id = generateNextInstanceId();
    auto instance_coupon = coupon->cloneInstance(instance_id);
    customer.addCoupon(std::move(instance_coupon));
    return core::Result::ok("优惠券购买成功");
}

std::shared_ptr<Coupon> CouponManager::findCouponInPool(const std::string &template_id) const
{
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getTemplateId() == template_id)
            return coupon;
    return nullptr;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getFreeCoupons() const
{
    std::vector<std::shared_ptr<Coupon>> free_coupons;
    for (const auto& coupon : public_pool)
        if (coupon && coupon->isFree())
            free_coupons.push_back(coupon);
    return free_coupons;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getPaidCoupons() const
{
    std::vector<std::shared_ptr<Coupon>> paid_coupons;
    for (const auto& coupon : public_pool)
        if (coupon && !coupon->isFree())
            paid_coupons.push_back(coupon);
    return paid_coupons;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getCouponsByCategory(core::ProductCategory category) const
{
    if (category == core::ProductCategory::ALL)
        return public_pool;
    std::vector<std::shared_ptr<Coupon>> coupons_by_category;
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getTargetCategory() == category)
            coupons_by_category.push_back(coupon);
    return coupons_by_category;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getCouponsByTier(core::UserTier tier) const
{
    std::vector<std::shared_ptr<Coupon>> coupons_by_tier;
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getMinTier() == tier)
            coupons_by_tier.push_back(coupon);
    return coupons_by_tier;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getEligibleCouponsForUser(const user::CustomerAccount& customer) const
{
    std::vector<std::shared_ptr<Coupon>> eligible_coupons;
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getMinTier() <= customer.getTier())
            eligible_coupons.push_back(coupon);
    return eligible_coupons;
}

std::vector<std::shared_ptr<Coupon>> CouponManager::getCouponsByTimeConstraint(core::TimeConstraint time_limit) const
{
    std::vector<std::shared_ptr<Coupon>> coupons_by_time;
    for (const auto& coupon : public_pool)
        if (coupon && coupon->getTimeConstraint() == time_limit)
            coupons_by_time.push_back(coupon);
    return coupons_by_time;
}

std::string CouponManager::generateNextInstanceId()
{
    return "INST_" + std::to_string(++instance_counter);
}

nlohmann::json CouponManager::toJson() const
{
    nlohmann::json json;
    json["instance_counter"] = instance_counter;
    nlohmann::json array = nlohmann::json::array();
    for (const auto& coupon : public_pool)
    {
        if (coupon)
            array.push_back(coupon->toJson());
    }
    json["public_pool"] = array;
    return json;
}

bool CouponManager::loadFromJson(const nlohmann::json& json)
{
    try
    {
        instance_counter = json.value("instance_counter", 1000ULL);
        public_pool.clear();

        for (const auto& item : json.at("public_pool"))
        {
            auto coupon = persist::CouponFactory::fromJson(item);
            if (coupon)
                public_pool.push_back(coupon);
        }
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

CouponManager CouponManager::fromJson(const nlohmann::json& json)
{
    CouponManager manager;
    manager.loadFromJson(json);
    return manager;
}

} // namespace shop
