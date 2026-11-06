#pragma once

#include "coupon/CouponPlanner.hpp"
#include "persist/StorageManager.hpp"
#include <optional>

namespace ui {

// The UI calls these operations; drawing and keyboard handling never perform
// partial payments. Public managers also keep the existing project API usable.
struct CheckoutQuote {
    shop::PlanResult plan;
    std::string fingerprint;
    std::string coupon_id;
};

struct CouponDraft {
    std::string id, name;
    int type = 0; // 0: threshold, 1: percentage discount, 2: cash
    double cost = 0, minimum = 0, value = 0, cap = 0;
    core::ProductCategory category = core::ProductCategory::ALL;
    core::UserTier tier = core::UserTier::V0_NORMAL;
    core::TimeConstraint time = core::TimeConstraint::ANYTIME;
};

class AppContext {
public:
    explicit AppContext(std::filesystem::path data_dir);
    user::UserManager user_mgr;
    shop::ProductManager prod_mgr;
    shop::CouponManager cpn_mgr;
    shop::Cart cart;
    persist::StorageManager storage_mgr;
    core::DayOfWeek current_day = core::DayOfWeek::WEDNESDAY;
    bool unsaved_changes = false;

    core::Result initialize();
    core::Result save();
    core::Result registerCustomer(const std::string& name, const std::string& password);
    core::Result login(const std::string& name, const std::string& password, bool admin);
    void logout();
    void toggleDay();
    core::Result addToCart(const std::string& id, int quantity);
    core::Result setCartQuantity(const std::string& id, int quantity);
    core::Result recharge(double amount);
    core::Result acquireCoupon(const std::string& id);

    // nullopt: automatic recommendation; empty string: explicitly no coupon.
    CheckoutQuote quote(std::optional<std::string> selection = std::nullopt) const;
    core::Result pay(const CheckoutQuote& displayed_quote);

    core::Result addProduct(const std::string& id, const std::string& name,
                            core::ProductCategory category, double price, int stock);
    core::Result replenish(const std::string& id, int count);
    core::Result issueCoupon(const CouponDraft& draft);
    core::Result changeTier(const std::string& name, core::UserTier tier);
    core::Result reset(const std::string& confirmation);

private:
    std::string checkoutFingerprint() const;
    core::Result changed(core::Result result);
};

std::string trim(const std::string& text);
bool validText(const std::string& text, std::size_t max_length = 120);
bool parseQuantity(const std::string& text, int& value, bool allow_zero = false);
bool parseAmount(const std::string& text, double& value, bool allow_zero = false);
std::string money(double amount);

} // namespace ui
