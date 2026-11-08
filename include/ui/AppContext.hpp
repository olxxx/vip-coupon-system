#pragma once

#include "common/Result.hpp"
#include "common/Types.hpp"
#include "coupon/CouponManager.hpp"
#include "coupon/CouponPlanner.hpp"
#include "persist/StorageManager.hpp"
#include "shop/Cart.hpp"
#include "shop/ProductManager.hpp"
#include "user/UserManager.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace ui {

// 结算支付成功回执单数据 (供 CheckoutPage 展示支付成果与等级晋升)
struct PaymentReceipt {
    bool valid = false;
    std::string order_id;
    std::string coupon_name;
    double origin_total = 0.0;
    double member_saving = 0.0;
    double coupon_saving = 0.0;
    double final_paid = 0.0;
    double balance_before = 0.0;
    double balance_after = 0.0;
    std::string old_tier_name;
    std::string upgraded_tier_name; // 非空表示发生等级跃迁
};

// 管理员发券表单草稿
struct CouponDraft {
    std::string id;
    std::string name;
    int type = 0; // 0: 满减券, 1: 折扣券, 2: 立减券
    double cost = 0.0;
    double min_spend = 0.0;
    double value = 0.0;       // 减免额 或 折扣率(0~1)
    double max_discount = 0.0;// 折扣券封顶额 (0 表示不封顶)
    core::ProductCategory category = core::ProductCategory::ALL;
    core::UserTier min_tier = core::UserTier::V0_NORMAL;
    core::TimeConstraint time_limit = core::TimeConstraint::ANYTIME;
};

// 全局共享上下文 (对齐 DESIGN.md §10.1，聚合各模块管理器与虚拟时间)
struct AppContext {
    user::UserManager& user_mgr;
    shop::ProductManager& prod_mgr;
    shop::CouponManager& cpn_mgr;
    shop::Cart& cart;
    persist::StorageManager& storage_mgr;

    // 模拟时间控制 (默认周三工作日，按 T 键切换周六周末)
    core::DayOfWeek current_day = core::DayOfWeek::WEDNESDAY;

    // 页面历史栈状态快照 (由 PageManager 维护，供顶栏与底栏展示)
    std::string prev_page_name;
    std::size_t stack_depth = 1;

    AppContext(user::UserManager& u,
               shop::ProductManager& p,
               shop::CouponManager& c,
               shop::Cart& cart_ref,
               persist::StorageManager& s)
        : user_mgr(u), prod_mgr(p), cpn_mgr(c), cart(cart_ref), storage_mgr(s) {}

    // --- 虚拟时间切换 ---
    void toggleWeekendSimulation();
    std::string getDayLabel() const;
    bool isWeekend() const { return core::isWeekend(current_day); }

    // --- 账户与会话编排 ---
    core::Result loginCustomer(const std::string& username, const std::string& password);
    core::Result loginAdmin(const std::string& username, const std::string& password);
    core::Result registerCustomer(const std::string& username, const std::string& password);
    void logout();

    // --- 消费者前台业务编排 ---
    core::Result addToCart(const std::string& product_id, int quantity = 1);
    core::Result changeCartQty(const std::string& product_id, int delta);
    core::Result removeCartItem(const std::string& product_id);
    core::Result clearCart();

    core::Result recharge(double amount, const std::string& channel = "个人中心充值");
    core::Result claimOrBuyCoupon(const std::string& template_id);
    core::Result claimAllFreeCoupons();
    core::Result cleanUsedCoupons();

    // 结算报价与原子支付
    shop::PlanResult computeQuote(const std::shared_ptr<shop::Coupon>& manual_coupon,
                                  bool use_optimal) const;
    core::Result pay(const std::shared_ptr<shop::Coupon>& manual_coupon,
                     bool use_optimal,
                     PaymentReceipt* out_receipt = nullptr);

    // --- 管理员后台业务编排 (内建 RBAC 权限校验) ---
    core::Result replenishProduct(const std::string& id, int count);
    core::Result setProductStock(const std::string& id, int stock);
    core::Result updateProductPrice(const std::string& id, double price);
    core::Result removeProduct(const std::string& id);
    core::Result addProduct(const std::string& id,
                            const std::string& name,
                            core::ProductCategory category,
                            double price,
                            int stock);

    core::Result issueCoupon(const CouponDraft& draft);
    core::Result removeCoupon(const std::string& template_id);

    core::Result updateCustomerTier(const std::string& username, core::UserTier new_tier);
    core::Result removeCustomer(const std::string& username);
    core::Result resetSystem();

private:
    core::Result persistAfter(core::Result res);
};

// --- 字符串与数值安全解析工具 ---
std::string trim(const std::string& text);
bool validText(const std::string& text, std::size_t max_len = 64);
bool parseQuantity(const std::string& text, int& out_value, bool allow_zero = false);
bool parseAmount(const std::string& text, double& out_value, bool allow_zero = false);
std::string money(double amount);

} // namespace ui
