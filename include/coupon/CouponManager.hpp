#pragma once
#include "common/Types.hpp"
#include "common/Result.hpp"
#include "user/CustomerAccount.hpp"
#include "coupon/Coupon.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace shop
{

class CouponManager
{
private:
    std::vector<std::shared_ptr<Coupon>> public_pool;
    uint64_t instance_counter=1000;

public:
    CouponManager() = default;

    //规模与状态
    size_t size() const { return public_pool.size(); }
    bool empty() const { return public_pool.empty(); }
    void clear() { public_pool.clear(); }
    bool hasCoupon(const std::string& template_id) const;

    //券池管理
    bool registerCouponToPool(std::shared_ptr<Coupon> coupon);
    bool removeCouponFromPool(const std::string& template_id);

    //事务编排
    core::Result claimFreeCoupon(const std::string& template_id,user::CustomerAccount& customer);
    core::Result buyPaidCoupon(const std::string& template_id, user::CustomerAccount& customer);

    //检索与视图
    std::shared_ptr<Coupon> findCouponInPool(const std::string& template_id) const;
    const std::vector<std::shared_ptr<Coupon>>& getPublicPool() const { return public_pool; }
    std::vector<std::shared_ptr<Coupon>> getFreeCoupons() const;
    std::vector<std::shared_ptr<Coupon>> getPaidCoupons() const;
    std::vector<std::shared_ptr<Coupon>> getCouponsByCategory(core::ProductCategory category) const;
    std::vector<std::shared_ptr<Coupon>> getCouponsByTier(core::UserTier tier) const;
    std::vector<std::shared_ptr<Coupon>> getEligibleCouponsForUser(const user::CustomerAccount& customer) const;
    std::vector<std::shared_ptr<Coupon>> getCouponsByTimeConstraint(core::TimeConstraint time_limit) const;

    //计数器管理
    uint64_t getInstanceCounter() const { return instance_counter; }
    void setInstanceCounter(uint64_t counter) { instance_counter = counter; }
    std::string generateNextInstanceId();

    //JSON
    nlohmann::json toJson() const;
    bool loadFromJson(const nlohmann::json& json);
    static CouponManager fromJson(const nlohmann::json& json);
};

}
