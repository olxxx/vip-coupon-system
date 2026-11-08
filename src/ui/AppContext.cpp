#include "ui/AppContext.hpp"
#include "coupon/ConcreteCoupons.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ui {

namespace {
constexpr double kMaxAmount = 100000000.0;
constexpr int kMaxQuantity = 1000000;

bool isValidAmountValue(double val, bool allow_zero) {
    return std::isfinite(val) && val <= kMaxAmount && (allow_zero ? val >= 0.0 : val > 0.0);
}
} // namespace

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool validText(const std::string& text, std::size_t max_len) {
    const auto s = trim(text);
    if (s.empty() || s.size() > max_len) return false;
    return std::none_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 32 || c == 127;
    });
}

bool parseQuantity(const std::string& text, int& out_value, bool allow_zero) {
    const auto input = trim(text);
    if (input.empty() || input.size() > 7) return false;
    int parsed = 0;
    for (unsigned char c : input) {
        if (c < '0' || c > '9') return false;
        parsed = parsed * 10 + (c - '0');
        if (parsed > kMaxQuantity) return false;
    }
    if (!allow_zero && parsed <= 0) return false;
    out_value = parsed;
    return true;
}

bool parseAmount(const std::string& text, double& out_value, bool allow_zero) {
    const auto input = trim(text);
    if (input.empty() || input.size() > 12) return false;
    if (input.front() == '.' || input.back() == '.') return false;

    bool has_dot = false;
    int decimals = 0;
    double parsed = 0.0;
    double fraction = 0.1;

    for (unsigned char c : input) {
        if (c == '.' && !has_dot) {
            has_dot = true;
            continue;
        }
        if (c < '0' || c > '9') return false;
        if (has_dot) {
            if (++decimals > 2) return false;
            parsed += (c - '0') * fraction;
            fraction *= 0.1;
        } else {
            parsed = parsed * 10.0 + (c - '0');
        }
    }
    if (!isValidAmountValue(parsed, allow_zero)) return false;
    out_value = parsed;
    return true;
}

std::string money(double amount) {
    std::ostringstream oss;
    oss << "¥" << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}

core::Result AppContext::persistAfter(core::Result res) {
    if (!res.success) return res;
    if (!storage_mgr.saveAll(user_mgr, prod_mgr, cpn_mgr)) {
        res.message += " (警告：磁盘写入失败)";
    }
    return res;
}

void AppContext::toggleWeekendSimulation() {
    if (core::isWeekend(current_day)) {
        current_day = core::DayOfWeek::WEDNESDAY;
    } else {
        current_day = core::DayOfWeek::SATURDAY;
    }
}

std::string AppContext::getDayLabel() const {
    return core::isWeekend(current_day) ? "周六 (周末)" : "周三 (工作日)";
}

core::Result AppContext::loginCustomer(const std::string& username, const std::string& password) {
    auto res = user_mgr.loginCustomer(trim(username), password);
    if (res.success) {
        cart.clear();
        res.message = "欢迎回来，" + trim(username) + "！按 ↑/↓ 选择商品，Enter 加入购物车。";
    }
    return res;
}

core::Result AppContext::loginAdmin(const std::string& username, const std::string& password) {
    auto res = user_mgr.loginAdmin(trim(username), password);
    if (res.success) {
        cart.clear();
        const auto adm = user_mgr.getCurrentAdmin();
        res.message = "已登录为 " + adm->getUsername() + " (" + adm->getRoleName() + ")。";
    }
    return res;
}

core::Result AppContext::registerCustomer(const std::string& username, const std::string& password) {
    const auto u = trim(username);
    if (!validText(u, 32) || !validText(password, 64)) {
        return core::Result::fail("用户名和密码不能为空（且不含控制字符）");
    }
    auto res = user_mgr.registerCustomer(u, password);
    if (!res.success) return res;

    if (auto cust = user_mgr.findCustomer(u)) {
        cust->addTransaction(std::make_unique<user::RechargeRecord>(
            user::TransactionRecord::generateId("RECHARGE"),
            core::getCurrentTimestamp(),
            cust->getBalance(),
            "新用户注册初始资金"
        ));
    }
    res.message = "账号 " + u + " 注册成功（初始余额 ¥1000.00），请点击 [ 消费者登录 ]。";
    return persistAfter(std::move(res));
}

void AppContext::logout() {
    cart.clear();
    user_mgr.logout();
}

core::Result AppContext::addToCart(const std::string& product_id, int quantity) {
    if (!user_mgr.isCustomerLoggedIn()) {
        return core::Result::fail("请先登录消费者账号");
    }
    auto prod = prod_mgr.findProduct(product_id);
    if (!prod) {
        return core::Result::fail("未找到该商品");
    }
    if (quantity <= 0) {
        return core::Result::fail("加购数量必须大于 0");
    }
    const int in_cart = cart.getItemQuantity(product_id);
    if (in_cart + quantity > prod->getStock()) {
        return core::Result::fail("无法加购：" + prod->getName() + " 库存不足 (剩余库存 " +
                                  std::to_string(prod->getStock()) + "，已加购 " +
                                  std::to_string(in_cart) + ")");
    }
    if (!cart.addItem(prod, quantity)) {
        return core::Result::fail("加入购物车失败");
    }
    return core::Result::ok("已将 " + prod->getName() + " x" + std::to_string(quantity) +
                            " 加入购物车。当前共 " + std::to_string(cart.getTotalQuantity()) +
                            " 件，合计 " + money(cart.getTotalOriginalAmount()));
}

core::Result AppContext::changeCartQty(const std::string& product_id, int delta) {
    if (!user_mgr.isCustomerLoggedIn()) {
        return core::Result::fail("请先登录消费者账号");
    }
    if (!cart.hasItem(product_id)) {
        return core::Result::fail("购物车中无此商品");
    }
    auto prod = prod_mgr.findProduct(product_id);
    const std::string name = prod ? prod->getName() : product_id;
    const int cur = cart.getItemQuantity(product_id);

    if (delta > 0) {
        if (prod && cur + delta > prod->getStock()) {
            return core::Result::fail("库存不足：" + name + " 库存仅 " +
                                      std::to_string(prod->getStock()) + " 件");
        }
        cart.increaseQuantity(product_id, delta);
        return core::Result::ok("已将 " + name + " 数量调整为 " +
                                std::to_string(cart.getItemQuantity(product_id)));
    } else if (delta < 0) {
        const int step = -delta;
        if (cur - step <= 0) {
            cart.removeItem(product_id);
            return core::Result::ok("已移除商品 " + name);
        }
        cart.decreaseQuantity(product_id, step);
        return core::Result::ok("已将 " + name + " 数量调整为 " +
                                std::to_string(cart.getItemQuantity(product_id)));
    }
    return core::Result::ok();
}

core::Result AppContext::removeCartItem(const std::string& product_id) {
    if (!cart.hasItem(product_id)) {
        return core::Result::fail("请先选择要移除的商品");
    }
    auto prod = prod_mgr.findProduct(product_id);
    const std::string name = prod ? prod->getName() : product_id;
    cart.removeItem(product_id);
    return core::Result::ok("已移除选中商品：" + name);
}

core::Result AppContext::clearCart() {
    cart.clear();
    return core::Result::ok("已清空购物车");
}

core::Result AppContext::recharge(double amount, const std::string& channel) {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return core::Result::fail("请先登录消费者账号");
    if (!isValidAmountValue(amount, false) || cust->getBalance() + amount > kMaxAmount) {
        return core::Result::fail("请输入有效的充值金额（大于 0，上限 1 亿元）");
    }
    cust->recharge(amount, channel);
    return persistAfter(core::Result::ok("充值成功：已到账 +" + money(amount) +
                                         "，当前最新余额 " + money(cust->getBalance())));
}

core::Result AppContext::claimOrBuyCoupon(const std::string& template_id) {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return core::Result::fail("请先登录消费者账号");
    auto tpl = cpn_mgr.findCouponInPool(template_id);
    if (!tpl) return core::Result::fail("未找到该优惠券模板");

    auto res = tpl->isFree() ? cpn_mgr.claimFreeCoupon(template_id, *cust)
                             : cpn_mgr.buyPaidCoupon(template_id, *cust);
    return persistAfter(std::move(res));
}

core::Result AppContext::claimAllFreeCoupons() {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return core::Result::fail("请先登录消费者账号");
    auto res = cpn_mgr.claimAllFreeCoupons(*cust);
    return persistAfter(std::move(res));
}

core::Result AppContext::cleanUsedCoupons() {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return core::Result::fail("请先登录消费者账号");
    const std::size_t before = cust->getCouponCount();
    cust->cleanUsedCoupons();
    const std::size_t removed = before - cust->getCouponCount();
    return persistAfter(core::Result::ok("已清理 " + std::to_string(removed) + " 张已核销优惠券"));
}

shop::PlanResult AppContext::computeQuote(const std::shared_ptr<shop::Coupon>& manual_coupon,
                                          bool use_optimal) const {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return {};
    if (use_optimal) {
        return shop::CouponPlanner::computeOptimalPlan(
            cart, cust->getCoupons(), cust->getTier(), current_day);
    }
    return shop::CouponPlanner::computeManualPlan(
        cart, manual_coupon, cust->getTier(), current_day);
}

core::Result AppContext::pay(const std::shared_ptr<shop::Coupon>& manual_coupon,
                             bool use_optimal,
                             PaymentReceipt* out_receipt) {
    auto cust = user_mgr.getCurrentCustomer();
    if (!cust) return core::Result::fail("请先登录消费者账号");
    if (cart.empty()) return core::Result::fail("购物车为空，无法结算");

    // 1. 严格校验库存
    std::string stock_err;
    if (!cart.validateStockAvailable(stock_err)) {
        return core::Result::fail("支付失败：" + stock_err);
    }

    // 2. 若手动指定了优惠券，校验其当前可用性
    if (!use_optimal && manual_coupon) {
        auto chk = shop::CouponPlanner::evaluateEligbility(
            *manual_coupon, cart, cust->getTier(), current_day);
        if (!chk.is_eligible) {
            return core::Result::fail("所选优惠券当前不可用：" + chk.fail_reason);
        }
    }

    // 3. 计算最终结算方案并校验余额
    const auto plan = computeQuote(manual_coupon, use_optimal);
    if (!core::isAmountGTE(cust->getBalance(), plan.final_pay_amount)) {
        const double diff = plan.final_pay_amount - cust->getBalance();
        return core::Result::fail("支付失败：余额不足 (需 " + money(plan.final_pay_amount) +
                                  "，还差 " + money(diff) + ")，请先前往个人中心充值");
    }

    // 4. 依次执行库存扣减、券核销、余额扣款与等级自动升级
    const double bal_before = cust->getBalance();
    const auto old_tier = cust->getTier();
    const std::string old_tier_name = cust->getTierName();
    const std::string order_id = user::TransactionRecord::generateId("ORD");

    if (!cart.deductAllStock()) {
        return core::Result::fail("扣减商品库存失败");
    }
    if (plan.best_coupon) {
        cust->consumeCoupon(plan.best_coupon->getInstanceId());
    }
    cust->deductBalance(plan.final_pay_amount, order_id);

    const bool upgraded = (cust->getTier() != old_tier);
    if (out_receipt) {
        out_receipt->valid = true;
        out_receipt->order_id = order_id;
        out_receipt->coupon_name = plan.best_coupon ? plan.best_coupon->getName() : "未使用优惠券";
        out_receipt->origin_total = plan.origin_total;
        out_receipt->member_saving = plan.member_discount_saving;
        out_receipt->coupon_saving = plan.coupon_discount_saving;
        out_receipt->final_paid = plan.final_pay_amount;
        out_receipt->balance_before = bal_before;
        out_receipt->balance_after = cust->getBalance();
        out_receipt->old_tier_name = old_tier_name;
        out_receipt->upgraded_tier_name = upgraded ? cust->getTierName() : "";
    }

    std::string msg = "订单 " + order_id + " 支付成功 (实付 " + money(plan.final_pay_amount) + ")！";
    if (upgraded) {
        msg += " 恭喜晋升为【" + cust->getTierName() + "】！";
    }
    return persistAfter(core::Result::ok(std::move(msg)));
}

core::Result AppContext::replenishProduct(const std::string& id, int count) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canManageInventory()) return core::Result::fail("无商品库存管理权限");
    auto prod = prod_mgr.findProduct(id);
    if (!prod) return core::Result::fail("未找到选中商品");
    if (count <= 0 || count > kMaxQuantity - prod->getStock()) {
        return core::Result::fail("补货数量必须为正整数，且总库存不超过 1000000");
    }
    if (!prod_mgr.replenishProduct(id, count)) {
        return core::Result::fail("补货操作失败");
    }
    return persistAfter(core::Result::ok("已为 " + prod->getName() + " 补货 +" +
                                         std::to_string(count) + " 件，当前库存 " +
                                         std::to_string(prod->getStock()) + " 件"));
}

core::Result AppContext::setProductStock(const std::string& id, int stock) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canManageInventory()) return core::Result::fail("无商品库存管理权限");
    auto prod = prod_mgr.findProduct(id);
    if (!prod) return core::Result::fail("未找到选中商品");
    if (stock < 0 || stock > kMaxQuantity) {
        return core::Result::fail("库存数量必须在 0 ~ 1000000 之间");
    }
    if (!prod_mgr.setStock(id, stock)) {
        return core::Result::fail("设置库存失败");
    }
    return persistAfter(core::Result::ok("已将 " + prod->getName() + " 库存设置为 " +
                                         std::to_string(stock) + " 件"));
}

core::Result AppContext::updateProductPrice(const std::string& id, double price) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canManageInventory()) return core::Result::fail("无商品管理权限");
    auto prod = prod_mgr.findProduct(id);
    if (!prod) return core::Result::fail("未找到选中商品");
    if (!isValidAmountValue(price, false)) {
        return core::Result::fail("商品单价必须大于 0");
    }
    if (!prod_mgr.updatePrice(id, price)) {
        return core::Result::fail("修改单价失败");
    }
    return persistAfter(core::Result::ok("已将 " + prod->getName() + " 单价更新为 " + money(price)));
}

core::Result AppContext::removeProduct(const std::string& id) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canManageInventory()) return core::Result::fail("无商品管理权限");
    auto prod = prod_mgr.findProduct(id);
    if (!prod) return core::Result::fail("未找到选中商品");
    const std::string name = prod->getName();
    if (!prod_mgr.removeProduct(id)) {
        return core::Result::fail("下架商品失败");
    }
    cart.removeItem(id);
    return persistAfter(core::Result::ok("已下架商品：" + name));
}

core::Result AppContext::addProduct(const std::string& id,
                                    const std::string& name,
                                    core::ProductCategory category,
                                    double price,
                                    int stock) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canManageInventory()) return core::Result::fail("无商品上架权限");
    const auto pid = trim(id);
    const auto pname = trim(name);
    if (!validText(pid, 32) || !validText(pname, 64)) {
        return core::Result::fail("请填写合法的商品编号与名称");
    }
    if (category == core::ProductCategory::ALL) {
        return core::Result::fail("请选择具体的商品分类");
    }
    if (!isValidAmountValue(price, false) || stock < 0 || stock > kMaxQuantity) {
        return core::Result::fail("单价必须大于 0，初始库存需为非负整数");
    }
    if (prod_mgr.hasProduct(pid)) {
        return core::Result::fail("上架失败：商品编号 " + pid + " 已存在");
    }
    if (!prod_mgr.addProduct(pid, pname, category, price, stock)) {
        return core::Result::fail("上架商品失败");
    }
    return persistAfter(core::Result::ok("已上架新商品 [" + pid + "] " + pname + " (" +
                                         core::getCategoryName(category) + ")"));
}

core::Result AppContext::issueCoupon(const CouponDraft& d) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canIssueCoupons()) return core::Result::fail("无发行优惠券权限");
    const auto cid = trim(d.id);
    const auto cname = trim(d.name);
    if (!validText(cid, 48) || !validText(cname, 64)) {
        return core::Result::fail("请填写有效的模板编号与券名称");
    }
    if (!isValidAmountValue(d.cost, true) || !isValidAmountValue(d.min_spend, true) ||
        !isValidAmountValue(d.value, false) || !isValidAmountValue(d.max_discount, true)) {
        return core::Result::fail("券参数数值无效");
    }
    if (d.type == 1 && (d.value <= 0.0 || d.value >= 1.0)) {
        return core::Result::fail("折扣券的折扣系数需在 (0, 1) 之间，如 0.85 表示 85 折");
    }

    std::shared_ptr<shop::Coupon> cpn;
    if (d.type == 0) {
        cpn = std::make_shared<shop::ThresholdCoupon>(
            cid, cname, d.cost, d.min_spend, d.category, d.min_tier, d.time_limit, d.value);
    } else if (d.type == 1) {
        cpn = std::make_shared<shop::DiscountCoupon>(
            cid, cname, d.cost, d.min_spend, d.value, d.category, d.min_tier, d.time_limit, d.max_discount);
    } else {
        cpn = std::make_shared<shop::CashCoupon>(
            cid, cname, d.cost, d.value, d.category, d.min_tier, d.time_limit);
    }

    const bool existed = cpn_mgr.hasCoupon(cid);
    cpn_mgr.registerCouponToPool(std::move(cpn));
    return persistAfter(core::Result::ok(existed ? ("已更新优惠券模板：" + cname)
                                                 : ("已发布新优惠券模板至领券中心：" + cname)));
}

core::Result AppContext::removeCoupon(const std::string& template_id) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm || !adm->canIssueCoupons()) return core::Result::fail("无管理优惠券权限");
    auto cpn = cpn_mgr.findCouponInPool(template_id);
    if (!cpn) return core::Result::fail("未找到选中的优惠券模板");
    const std::string cname = cpn->getName();
    if (!cpn_mgr.removeCouponFromPool(template_id)) {
        return core::Result::fail("下架优惠券模板失败");
    }
    return persistAfter(core::Result::ok("已从公共池下架优惠券模板：" + cname));
}

core::Result AppContext::updateCustomerTier(const std::string& username, core::UserTier new_tier) {
    auto res = user_mgr.updateCustomerTier(username, new_tier);
    if (res.success) {
        res.message = "已将用户 " + username + " 的会员等级调整为【" +
                      core::getTierConfig(new_tier).name + "】";
    }
    return persistAfter(std::move(res));
}

core::Result AppContext::removeCustomer(const std::string& username) {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm) return core::Result::fail("请先以管理员身份登录");
    if (!adm->canModifyUserTier()) {
        return core::Result::fail("权限不足：仅超级管理员 (root) 可删除消费者账号");
    }
    if (!user_mgr.removeCustomer(username)) {
        return core::Result::fail("目标消费者账号不存在");
    }
    return persistAfter(core::Result::ok("已删除消费者账号：" + username));
}

core::Result AppContext::resetSystem() {
    auto adm = user_mgr.getCurrentAdmin();
    if (!adm) return core::Result::fail("请先以管理员身份登录");
    if (!adm->canResetSystem()) {
        return core::Result::fail("权限不足：仅超级管理员 (root) 可重置系统数据");
    }
    cart.clear();
    if (!storage_mgr.resetToDefault(user_mgr, prod_mgr, cpn_mgr)) {
        return core::Result::fail("重置系统数据失败，请检查文件权限");
    }
    return core::Result::ok("系统已恢复初始演示数据");
}

} // namespace ui
