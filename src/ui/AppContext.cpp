#include "ui/AppContext.hpp"
#include "coupon/ConcreteCoupons.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace ui {
namespace {
constexpr double kMaxAmount = 100000000.0;
constexpr int kMaxQuantity = 1000000;
bool amountValid(double value, bool zero = false) {
    return std::isfinite(value) && value <= kMaxAmount &&
           (zero ? value >= 0 : value > 0);
}
bool categoryValid(core::ProductCategory category, bool all) {
    const int value = static_cast<int>(category);
    return value >= (all ? 0 : 1) && value <= 5;
}
} // namespace

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

bool validText(const std::string& text, std::size_t limit) {
    return !trim(text).empty() && text.size() <= limit &&
        std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

bool parseQuantity(const std::string& text, int& value, bool zero) {
    const auto input = trim(text);
    if (input.empty() || input.size() > 7) return false;
    int parsed = 0;
    for (const unsigned char c : input) {
        if (c < '0' || c > '9') return false;
        parsed = parsed * 10 + c - '0';
        if (parsed > kMaxQuantity) return false;
    }
    if (!zero && parsed == 0) return false;
    value = parsed;
    return true;
}

bool parseAmount(const std::string& text, double& value, bool zero) {
    const auto input = trim(text);
    if (input.empty() || input.size() > 12) return false;
    bool dot = false;
    int decimals = 0;
    double parsed = 0, fraction = 0.1;
    if (input.front() == '.' || input.back() == '.') return false;
    for (const unsigned char c : input) {
        if (c == '.' && !dot) { dot = true; continue; }
        if (c < '0' || c > '9') return false;
        if (dot) {
            if (++decimals > 2) return false;
            parsed += (c - '0') * fraction;
            fraction *= 0.1;
        } else parsed = parsed * 10 + c - '0';
    }
    if (!amountValid(parsed, zero)) return false;
    value = parsed;
    return true;
}

std::string money(double amount) {
    std::ostringstream out;
    out << "¥" << std::fixed << std::setprecision(2) << amount;
    return out.str();
}

AppContext::AppContext(std::filesystem::path path) : storage_mgr(std::move(path)) {}

core::Result AppContext::initialize() {
    try {
        const int existing = static_cast<int>(std::filesystem::exists(storage_mgr.getUsersFilePath())) +
            static_cast<int>(std::filesystem::exists(storage_mgr.getProductsFilePath())) +
            static_cast<int>(std::filesystem::exists(storage_mgr.getCouponsFilePath()));
        // The storage module seeds on ANY missing file. Only call its reset on
        // an empty store, so a partial/corrupt store is never silently replaced.
        if (existing == 0) {
            if (!storage_mgr.resetToDefault(user_mgr, prod_mgr, cpn_mgr))
                return core::Result::fail("无法创建初始数据，请检查数据目录的写入权限。");
        } else if (existing != 3 || !storage_mgr.loadAll(user_mgr, prod_mgr, cpn_mgr)) {
            return core::Result::fail("数据文件缺失或损坏，已停止启动且未覆盖原文件。请恢复备份或使用新的 --data-dir。");
        }
        return core::Result::ok("数据已加载");
    } catch (const std::exception& e) {
        return core::Result::fail(std::string("初始化失败：") + e.what());
    }
}

core::Result AppContext::save() {
    unsaved_changes = !storage_mgr.saveAll(user_mgr, prod_mgr, cpn_mgr);
    return unsaved_changes ? core::Result::fail("保存失败，变更暂存于内存。请恢复写入权限后点击「保存」。")
                           : core::Result::ok("已保存全部数据");
}

core::Result AppContext::changed(core::Result result) {
    if (!result.success) return result;
    const auto saved = save();
    if (!saved.success) result.message += "；" + saved.message;
    return result; // Do not suggest retrying a completed purchase on save failure.
}

core::Result AppContext::registerCustomer(const std::string& name, const std::string& password) {
    if (!validText(name, 64) || !validText(password, 128))
        return core::Result::fail("请输入有效用户名和密码（用户名最多64字节，密码最多128字节）。");
    return changed(user_mgr.registerCustomer(trim(name), password));
}

core::Result AppContext::login(const std::string& name, const std::string& password, bool admin) {
    const auto result = admin ? user_mgr.loginAdmin(trim(name), password)
                              : user_mgr.loginCustomer(trim(name), password);
    if (result.success) cart.clear();
    return result;
}

void AppContext::logout() { cart.clear(); user_mgr.logout(); }
void AppContext::toggleDay() {
    current_day = core::isWeekend(current_day) ? core::DayOfWeek::WEDNESDAY : core::DayOfWeek::SATURDAY;
}

core::Result AppContext::addToCart(const std::string& id, int quantity) {
    if (!user_mgr.isCustomerLoggedIn()) return core::Result::fail("请先登录消费者账号");
    if (quantity <= 0 || quantity > kMaxQuantity - cart.getTotalQuantity())
        return core::Result::fail("数量必须为正整数，购物车总件数最多1000000");
    return cart.addItem(prod_mgr.findProduct(id), quantity) ? core::Result::ok("已加入购物车")
                                                         : core::Result::fail("商品不存在或库存不足");
}

core::Result AppContext::setCartQuantity(const std::string& id, int quantity) {
    if (!user_mgr.isCustomerLoggedIn()) return core::Result::fail("请先登录消费者账号");
    const auto product = prod_mgr.findProduct(id);
    if (!product || !cart.hasItem(id)) return core::Result::fail("请选择购物车中的商品");
    if (quantity < 0 || quantity > kMaxQuantity - (cart.getTotalQuantity() - cart.getItemQuantity(id)) ||
        (quantity > 0 && !product->hasStock(quantity))) return core::Result::fail("数量超出范围或库存不足");
    cart.updateQuantity(id, quantity);
    return core::Result::ok(quantity == 0 ? "已移出商品" : "数量已更新");
}

core::Result AppContext::recharge(double amount) {
    auto customer = user_mgr.getCurrentCustomer();
    if (!customer) return core::Result::fail("请先登录消费者账号");
    if (!amountValid(amount) || customer->getBalance() > kMaxAmount - amount)
        return core::Result::fail("充值金额无效，余额上限为100000000元");
    customer->recharge(amount, "TUI 模拟充值");
    return changed(core::Result::ok("模拟充值成功：" + money(amount)));
}

core::Result AppContext::acquireCoupon(const std::string& id) {
    auto customer = user_mgr.getCurrentCustomer();
    auto coupon = cpn_mgr.findCouponInPool(id);
    if (!customer) return core::Result::fail("请先登录消费者账号");
    if (!coupon) return core::Result::fail("请选择优惠券");
    return changed(coupon->isFree() ? cpn_mgr.claimFreeCoupon(id, *customer)
                                   : cpn_mgr.buyPaidCoupon(id, *customer));
}

std::string AppContext::checkoutFingerprint() const {
    const auto customer = user_mgr.getCurrentCustomer();
    if (!customer) return {};
    nlohmann::json state = { {"user", customer->getUsername()}, {"tier", static_cast<int>(customer->getTier())},
                            {"day", static_cast<int>(current_day)}, {"cart", nlohmann::json::array()} };
    for (const auto& item : cart.getItemslist()) {
        state["cart"].push_back({item.getProductId(), item.quantity, item.getProductPrice(),
                                 static_cast<int>(item.getProductCategory())});
    }
    return state.dump();
}

CheckoutQuote AppContext::quote(std::optional<std::string> selection) const {
    CheckoutQuote result;
    const auto customer = user_mgr.getCurrentCustomer();
    if (!customer) return result;
    result.fingerprint = checkoutFingerprint();
    result.plan = selection ? shop::CouponPlanner::computeManualPlan(cart, customer->findCoupon(*selection), customer->getTier(), current_day)
                            : shop::CouponPlanner::computeOptimalPlan(cart, customer->getCoupons(), customer->getTier(), current_day);
    result.coupon_id = selection ? *selection : (result.plan.best_coupon ? result.plan.best_coupon->getInstanceId() : "");
    return result;
}

core::Result AppContext::pay(const CheckoutQuote& displayed) {
    auto customer = user_mgr.getCurrentCustomer();
    if (!customer) return core::Result::fail("请先登录消费者账号");
    if (cart.empty()) return core::Result::fail("购物车为空，不能重复支付");
    if (displayed.fingerprint != checkoutFingerprint())
        return core::Result::fail("购物车、会员等级或模拟日期已变化，请重新核对账单后支付");
    std::string stock_error;
    if (!cart.validateStockAvailable(stock_error)) return core::Result::fail("库存不足，请返回购物车调整数量");
    if (!displayed.coupon_id.empty()) {
        const auto coupon = customer->findCoupon(displayed.coupon_id);
        if (!coupon) return core::Result::fail("所选优惠券不属于当前用户");
        const auto check = shop::CouponPlanner::evaluateEligbility(*coupon, cart, customer->getTier(), current_day);
        if (!check.is_eligible) return core::Result::fail("所选券不可用：" + check.fail_reason);
    }
    const auto current = quote(displayed.coupon_id);
    const auto& plan = current.plan;
    if (!std::isfinite(plan.final_pay_amount) || plan.final_pay_amount < 0 ||
        !std::isfinite(displayed.plan.final_pay_amount) ||
        std::abs(plan.final_pay_amount - displayed.plan.final_pay_amount) > 1e-4)
        return core::Result::fail("结算金额已变化，请重新核对账单");
    if (!core::isAmountGTE(customer->getBalance(), plan.final_pay_amount))
        return core::Result::fail("余额不足，还差 " + money(plan.final_pay_amount - customer->getBalance()) + "，请到个人中心充值");

    // Prepare the entire account update before touching live state. This also
    // reuses deductBalance's single expense record and automatic upgrade logic.
    auto staged = user::CustomerAccount::fromJson(customer->toJson());
    if (!staged) return core::Result::fail("账户数据异常，支付未执行");
    const auto old_tier = customer->getTier();
    const auto order_id = user::TransactionRecord::generateId("ORDER");
    if (!staged->deductBalance(plan.final_pay_amount, order_id) ||
        (!displayed.coupon_id.empty() && !staged->consumeCoupon(displayed.coupon_id)))
        return core::Result::fail("支付校验失败，未扣款");
    // No callbacks or asynchronous work between validation and stock deduction.
    if (!cart.deductAllStock()) return core::Result::fail("库存校验失败，未扣款");
    *customer = std::move(*staged);
    std::string message = "支付成功 " + money(plan.final_pay_amount) + " · " + order_id;
    if (customer->getTier() != old_tier) message += " · 已晋升" + customer->getTierName();
    return changed(core::Result::ok(message));
}

core::Result AppContext::addProduct(const std::string& id, const std::string& name,
                                   core::ProductCategory category, double price, int stock) {
    const auto admin = user_mgr.getCurrentAdmin();
    if (!admin || !admin->canManageInventory()) return core::Result::fail("没有商品管理权限");
    if (!validText(id, 64) || !validText(name) || !categoryValid(category, false) ||
        !amountValid(price) || stock < 0 || stock > kMaxQuantity)
        return core::Result::fail("商品信息无效：价格需大于0，库存需为0至1000000的整数");
    return changed(prod_mgr.addProduct(trim(id), trim(name), category, price, stock)
        ? core::Result::ok("商品已上架") : core::Result::fail("商品编号已存在"));
}

core::Result AppContext::replenish(const std::string& id, int count) {
    const auto admin = user_mgr.getCurrentAdmin();
    if (!admin || !admin->canManageInventory()) return core::Result::fail("没有补货权限");
    const auto product = prod_mgr.findProduct(id);
    if (!product || count <= 0 || count > kMaxQuantity - product->getStock())
        return core::Result::fail("商品不存在或补货数量超出库存上限1000000");
    return changed(prod_mgr.replenishProduct(id, count) ? core::Result::ok("补货成功") : core::Result::fail("补货失败"));
}

core::Result AppContext::issueCoupon(const CouponDraft& d) {
    const auto admin = user_mgr.getCurrentAdmin();
    if (!admin || !admin->canIssueCoupons()) return core::Result::fail("没有发券权限");
    if (!validText(d.id, 64) || !validText(d.name) || d.type < 0 || d.type > 2 ||
        !amountValid(d.cost, true) || !amountValid(d.minimum, true) || !amountValid(d.value) ||
        !amountValid(d.cap, true) || !categoryValid(d.category, true) ||
        static_cast<int>(d.tier) < 0 || static_cast<int>(d.tier) > 5 ||
        static_cast<int>(d.time) < 0 || static_cast<int>(d.time) > 2 ||
        (d.type == 1 && d.value >= 1.0)) return core::Result::fail("券参数无效，折扣比例需在0与1之间（如0.85）");
    if (cpn_mgr.hasCoupon(trim(d.id))) return core::Result::fail("券模板编号已存在，请使用新编号");
    std::shared_ptr<shop::Coupon> coupon;
    if (d.type == 0) coupon = std::make_shared<shop::ThresholdCoupon>(trim(d.id), trim(d.name), d.cost, d.minimum, d.category, d.tier, d.time, d.value);
    if (d.type == 1) coupon = std::make_shared<shop::DiscountCoupon>(trim(d.id), trim(d.name), d.cost, d.minimum, d.value, d.category, d.tier, d.time, d.cap);
    if (d.type == 2) coupon = std::make_shared<shop::CashCoupon>(trim(d.id), trim(d.name), d.cost, d.value, d.category, d.tier, d.time);
    cpn_mgr.registerCouponToPool(std::move(coupon));
    return changed(core::Result::ok("新券已发布到领券中心"));
}

core::Result AppContext::changeTier(const std::string& name, core::UserTier tier) {
    return changed(user_mgr.updateCustomerTier(name, tier));
}

core::Result AppContext::reset(const std::string& confirmation) {
    const auto admin = user_mgr.getCurrentAdmin();
    if (!admin || !admin->canResetSystem()) return core::Result::fail("仅超级管理员可以重置系统");
    if (confirmation != "RESET") return core::Result::fail("请输入大写 RESET 确认重置");
    cart.clear();
    unsaved_changes = !storage_mgr.resetToDefault(user_mgr, prod_mgr, cpn_mgr);
    return core::Result::ok(unsaved_changes ? "已在内存重置，但保存失败；请重试保存" : "系统已恢复演示数据，请重新登录");
}
} // namespace ui
