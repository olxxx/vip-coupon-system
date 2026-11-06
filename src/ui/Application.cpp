#include "ui/Application.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <utility>

namespace ui {
using namespace ftxui;
namespace {
const Color accent = Color::CyanLight;
Element panel(const std::string& title, Elements body) {
    body.insert(body.begin(), separator());
    body.insert(body.begin(), text(" " + title + " ") | bold | color(accent));
    return vbox(std::move(body)) | borderRounded;
}
Element row(const std::string& label, const Component& field) {
    return hbox({text(label) | size(WIDTH, EQUAL, 14), field->Render() | flex});
}
Element columns(Element left, Element right) {
    if (Terminal::Size().dimx < 96) return vbox({left, right});
    return hbox({left | flex, right | flex});
}
Element listView(const Component& list, int height = 9) {
    return list->Render() | vscroll_indicator | yframe | size(HEIGHT, LESS_THAN, height);
}
Component button(const std::string& label, std::function<void()> action) {
    return Button(label, std::move(action), ButtonOption::Border());
}
Component input(std::string& value, const std::string& placeholder, bool password = false) {
    InputOption option;
    option.password = password;
    option.multiline = false;
    return Input(&value, placeholder, option);
}
void clampIndex(int& index, std::size_t count) {
    index = count ? std::clamp(index, 0, static_cast<int>(count) - 1) : 0;
}
template<class T> std::shared_ptr<T> chosen(const std::vector<std::shared_ptr<T>>& items, int index) {
    return index >= 0 && index < static_cast<int>(items.size()) ? items[index] : nullptr;
}
std::string timeRule(core::TimeConstraint value) {
    if (value == core::TimeConstraint::WEEKDAY_ONLY) return "仅工作日";
    if (value == core::TimeConstraint::WEEKEND_ONLY) return "仅周末";
    return "不限时间";
}
Elements couponDetails(const std::shared_ptr<shop::Coupon>& coupon) {
    if (!coupon) return {text("暂无优惠券") | dim};
    return {paragraph(coupon->getName()) | bold,
            paragraph(coupon->getRuleDescription()),
            text("品类：" + core::getCategoryName(coupon->getTargetCategory())),
            text("等级：" + core::getTierConfig(coupon->getMinTier()).name + "及以上"),
            text("时间：" + timeRule(coupon->getTimeConstraint())),
            text("购券价格：" + (coupon->isFree() ? std::string("免费") : money(coupon->getCost())))};
}
} // namespace

struct Application::Impl {
    AppContext& ctx;
    std::function<void()> exit;
    Component root, tabs, nav, customer_nav, admin_nav;
    Components pages;
    std::vector<Components> focus_order = std::vector<Components>(7);
    std::vector<Components> admin_focus_order = std::vector<Components>(4);
    Components customer_actions, admin_actions;
    int active = 0, nav_active = 0;
    bool quit_dialog = false, error = false;
    std::string message = "欢迎使用。Tab 切换控件，方向键选择，Enter 确认。";
    std::vector<std::string> categories = {"全部", "数码", "服饰", "食品", "日用", "其他"};
    std::vector<std::string> product_categories = {"数码", "服饰", "食品", "日用", "其他"};
    std::vector<std::string> tiers = {"V0", "V1", "V2", "V3", "V4", "V5"};
    std::vector<std::string> times = {"不限", "工作日", "周末"};
    std::vector<std::string> login_modes = {"消费者登录", "注册新账号", "管理员登录"};
    int login_mode = 0;
    std::string username, password, repeated_password;
    std::string search, quantity = "1";
    int category = 0, product_selected = 0;
    std::vector<std::shared_ptr<shop::Product>> products;
    std::vector<std::string> product_entries;
    int cart_selected = 0;
    std::string cart_quantity = "1";
    std::vector<shop::CartItem> cart_items;
    std::vector<std::string> cart_entries;
    int checkout_selected = 0;
    std::vector<shop::CouponEvaluationItem> evaluated;
    std::vector<std::string> checkout_entries;
    CheckoutQuote preview;
    std::string recharge_amount = "100";
    int wallet_selected = 0, transaction_selected = 0;
    std::vector<std::shared_ptr<shop::Coupon>> wallet;
    std::vector<std::string> wallet_entries, transaction_entries;
    int square_selected = 0, square_filter = 0;
    std::vector<std::string> square_filters = {"全部", "免费券", "付费券"};
    std::vector<std::shared_ptr<shop::Coupon>> square;
    std::vector<std::string> square_entries;
    int admin_tab = 0, admin_user_selected = 0, admin_product_selected = 0, admin_coupon_selected = 0, new_tier = 0;
    std::vector<std::string> admin_tabs = {"用户", "商品", "发券", "系统"};
    std::vector<std::shared_ptr<user::CustomerAccount>> admin_users;
    std::vector<std::shared_ptr<shop::Product>> admin_products;
    std::vector<std::shared_ptr<shop::Coupon>> admin_coupons;
    std::vector<std::string> admin_user_entries, admin_product_entries, admin_coupon_entries;
    std::string product_id, product_name, product_price, product_stock = "0", restock_count = "10";
    int new_category = 0;
    std::vector<std::string> coupon_types = {"满减券", "折扣券", "立减券"};
    std::string coupon_id, coupon_name, coupon_cost = "0", coupon_minimum = "0", coupon_value, coupon_cap = "0";
    int coupon_type = 0, coupon_category = 0, coupon_tier = 0, coupon_time = 0;
    std::string reset_confirmation;

    Impl(AppContext& context, std::function<void()> quit) : ctx(context), exit(std::move(quit)) {
        pages = {loginPage(), mallPage(), cartPage(), checkoutPage(), userPage(), squarePage(), adminPage()};
        tabs = Container::Tab(pages, &active);
        auto mall = button("商城 F2", [this] { navigate(Page::Mall); });
        auto cart_button = button("购物车 F3", [this] { navigate(Page::Cart); });
        auto square_button = button("领券 F4", [this] { navigate(Page::CouponSquare); });
        auto user_button = button("个人 F5", [this] { navigate(Page::UserCenter); });
        auto admin_button = button("运营控制台", [this] { navigate(Page::Admin); });
        customer_nav = Container::Horizontal({mall, cart_button, square_button, user_button});
        admin_nav = Container::Horizontal({admin_button});
        nav = Container::Tab({Container::Horizontal({}), customer_nav, admin_nav}, &nav_active);
        auto date_button = button("日期 F6", [this] { toggleDay(); });
        auto save_button = button("保存", [this] { notify(ctx.save()); });
        auto logout_button = button("退出登录", [this] { logout(); });
        auto exit_button = button("退出程序", [this] { requestQuit(); });
        auto utilities = Container::Horizontal({date_button, save_button, logout_button, exit_button});
        customer_actions = {mall, cart_button, square_button, user_button, date_button, save_button, logout_button, exit_button};
        admin_actions = {admin_button, date_button, save_button, logout_button, exit_button};
        auto signed_in = Container::Vertical({nav, utilities});
        auto chrome = Maybe(signed_in, [this] { return ctx.user_mgr.isLoggedIn(); });
        auto layout = Container::Vertical({chrome, tabs});
        auto rendered = Renderer(layout, [this, chrome] {
            const auto customer = ctx.user_mgr.getCurrentCustomer();
            const auto admin = ctx.user_mgr.getCurrentAdmin();
            const auto account = customer ? customer->getDisplaySummary() : (admin ? admin->getDisplaySummary() : "未登录");
            const auto day = core::isWeekend(ctx.current_day) ? "周六 · 周末模拟" : "周三 · 工作日模拟";
            return vbox({
                hbox({text(" VIP CLUB ") | bold | color(Color::Black) | bgcolor(accent),
                      text(" 用户权益与优惠规划 ") | bold, filler(), text(day) | color(Color::YellowLight)}),
                paragraph(account + (ctx.unsaved_changes ? "  [有未保存变更]" : "")),
                chrome->Render(), separator(),
                tabs->Render() | vscroll_indicator | yframe | flex,
                separator(), paragraph(message) | color(error ? Color::RedLight : Color::GreenLight),
                text("Tab/Shift+Tab 切换  ↑↓选择  Enter确认  Esc返回  Ctrl+Q退出") | dim
            }) | border;
        });
        auto exit_buttons = Container::Horizontal({
            button("重试保存并退出", [this] { notify(ctx.save()); if (!ctx.unsaved_changes) exit(); }),
            button("放弃未保存变更并退出", [this] { exit(); }),
            button("继续使用", [this] { quit_dialog = false; })});
        auto modal = Renderer(exit_buttons, [exit_buttons] {
            return panel("尚有未保存数据", {paragraph("保存失败。退出会丢失尚未写入磁盘的变更。"), exit_buttons->Render()}) | clear_under | center;
        });
        root = CatchEvent(Modal(rendered, modal, &quit_dialog), [this](Event e) {
            if (e == Event::CtrlQ) { requestQuit(); return true; }
            if (quit_dialog) {
                if (e == Event::Escape) { quit_dialog = false; return true; }
                return false;
            }
            // FTXUI's nested menus consume Tab to cycle their own entries.
            // Keep Tab for moving between controls, and arrows for selection.
            if (e == Event::Tab || e == Event::TabReverse) { moveFocus(e == Event::TabReverse); return true; }
            if (e == Event::Escape) { back(); return true; }
            if (e == Event::F6 && ctx.user_mgr.isLoggedIn()) { toggleDay(); return true; }
            if (ctx.user_mgr.isCustomerLoggedIn()) {
                if (e == Event::F2) { navigate(Page::Mall); return true; }
                if (e == Event::F3) { navigate(Page::Cart); return true; }
                if (e == Event::F4) { navigate(Page::CouponSquare); return true; }
                if (e == Event::F5) { navigate(Page::UserCenter); return true; }
            }
            return false;
        });
        pages[0]->TakeFocus();
    }
    void notify(const core::Result& result) { message = result.message; error = !result.success || ctx.unsaved_changes; }
    void moveFocus(bool reverse) {
        Components order = focus_order[active];
        if (static_cast<Page>(active) == Page::Admin)
            order.insert(order.end(), admin_focus_order[admin_tab].begin(), admin_focus_order[admin_tab].end());
        const auto& actions = ctx.user_mgr.isCustomerLoggedIn() ? customer_actions : admin_actions;
        if (ctx.user_mgr.isLoggedIn()) order.insert(order.end(), actions.begin(), actions.end());
        if (order.empty()) return;
        int current = -1;
        for (std::size_t i = 0; i < order.size(); ++i) if (order[i]->Focused()) { current = static_cast<int>(i); break; }
        const int count = static_cast<int>(order.size());
        if (current == -1) current = reverse ? 0 : count - 1;
        for (int step = 1; step <= count; ++step) {
            const int index = (current + (reverse ? -step : step) + count * 2) % count;
            if (order[index]->Focusable()) { order[index]->TakeFocus(); return; }
        }
    }
    void navigate(Page target) {
        if (target == Page::Admin && !ctx.user_mgr.isAdminLoggedIn()) { notify(core::Result::fail("请先登录管理员账号")); return; }
        if (target != Page::Login && target != Page::Admin && !ctx.user_mgr.isCustomerLoggedIn()) {
            notify(core::Result::fail("请先登录消费者账号")); return;
        }
        if (target == Page::Checkout && ctx.cart.empty()) { notify(core::Result::fail("购物车为空，请先选购商品")); return; }
        if (target == Page::Login && ctx.user_mgr.isLoggedIn()) ctx.logout();
        active = static_cast<int>(target);
        nav_active = ctx.user_mgr.isCustomerLoggedIn() ? 1 : (ctx.user_mgr.isAdminLoggedIn() ? 2 : 0);
        refresh();
        pages[active]->TakeFocus();
    }
    void logout() {
        ctx.logout(); password.clear(); repeated_password.clear();
        notify(core::Result::ok("已退出登录，当前购物车已清空")); navigate(Page::Login);
    }
    void back() {
        const auto page = static_cast<Page>(active);
        if (page == Page::Checkout) navigate(Page::Cart);
        else if (page == Page::Mall || page == Page::Admin) logout();
        else if (page == Page::Login) requestQuit();
        else navigate(Page::Mall);
    }
    void requestQuit() {
        if (ctx.unsaved_changes) { notify(ctx.save()); quit_dialog = ctx.unsaved_changes; }
        if (!ctx.unsaved_changes) exit();
    }
    void toggleDay() { ctx.toggleDay(); refresh(); notify(core::Result::ok("模拟日期已切换，结算推荐已重新计算")); }
    void refreshProducts() {
        products.clear(); product_entries.clear();
        for (const auto& p : ctx.prod_mgr.getProductsByCategory(static_cast<core::ProductCategory>(category))) {
            if (p->getName().find(search) == std::string::npos && p->getId().find(search) == std::string::npos) continue;
            products.push_back(p);
            product_entries.push_back(p->getId() + "  " + p->getName() + "  " + money(p->getPrice()) + (p->isOutOfStock() ? " [售罄]" : ""));
        }
        clampIndex(product_selected, products.size());
    }
    void syncCartQuantity() { cart_quantity = cart_items.empty() ? "1" : std::to_string(cart_items[cart_selected].quantity); }
    void refreshCart() {
        cart_items = ctx.cart.getItemslist(); cart_entries.clear();
        for (const auto& item : cart_items) cart_entries.push_back(item.getProductName() + "  ×" + std::to_string(item.quantity) + "  " + money(item.getSubtotal()));
        clampIndex(cart_selected, cart_items.size()); syncCartQuantity();
    }
    std::string selectedCartId() const { return cart_items.empty() ? "" : cart_items[cart_selected].getProductId(); }
    void updateQuote() {
        if (checkout_selected == 0) preview = ctx.quote();
        else if (checkout_selected == 1) preview = ctx.quote(std::string{});
        else if (checkout_selected - 2 < static_cast<int>(evaluated.size())) preview = ctx.quote(evaluated[checkout_selected - 2].coupon->getInstanceId());
    }
    void refreshCheckout() {
        checkout_selected = 0; checkout_entries = {"自动选择最省钱方案", "不使用优惠券"}; evaluated.clear();
        if (const auto c = ctx.user_mgr.getCurrentCustomer()) {
            evaluated = shop::CouponPlanner::evaluateAndSortAllCoupons(ctx.cart, c->getCoupons(), c->getTier(), ctx.current_day);
            for (const auto& item : evaluated) {
                const auto state = item.check.is_eligible ? (item.is_recommended ? "[推荐] " : "[可用] ") : "[不可用] ";
                checkout_entries.push_back(state + item.coupon->getName() + "  " + item.coupon->getInstanceId());
            }
        }
        updateQuote();
    }
    void refreshUser() {
        wallet.clear(); wallet_entries.clear(); transaction_entries.clear();
        if (const auto c = ctx.user_mgr.getCurrentCustomer()) {
            for (const auto& cp : c->getCoupons()) if (cp) {
                wallet.push_back(cp);
                wallet_entries.push_back((cp->isUsed() ? "[已使用] " : "[未使用] ") + cp->getName() + " " + cp->getInstanceId());
            }
            for (const auto* tx : c->getRecentTransactions(c->getTransactionCount())) transaction_entries.push_back(tx->getFormattedText());
        }
        clampIndex(wallet_selected, wallet_entries.size()); clampIndex(transaction_selected, transaction_entries.size());
    }
    void refreshSquare() {
        square = square_filter == 1 ? ctx.cpn_mgr.getFreeCoupons() : square_filter == 2 ? ctx.cpn_mgr.getPaidCoupons() : ctx.cpn_mgr.getPublicPool();
        square_entries.clear();
        for (const auto& cp : square) square_entries.push_back(cp->getName() + "  " + (cp->isFree() ? "免费" : money(cp->getCost())));
        clampIndex(square_selected, square.size());
    }
    void refreshAdmin() {
        admin_users = ctx.user_mgr.getAllCustomers(); admin_user_entries.clear();
        for (const auto& c : admin_users) admin_user_entries.push_back(c->getDisplaySummary());
        admin_products = ctx.prod_mgr.getProducts(); admin_product_entries.clear();
        for (const auto& p : admin_products) admin_product_entries.push_back(p->getId() + " " + p->getName() + " · 库存 " + std::to_string(p->getStock()));
        admin_coupons = ctx.cpn_mgr.getPublicPool(); admin_coupon_entries.clear();
        for (const auto& cp : admin_coupons) admin_coupon_entries.push_back(cp->getTemplateId() + " " + cp->getName());
        clampIndex(admin_user_selected, admin_users.size()); clampIndex(admin_product_selected, admin_products.size()); clampIndex(admin_coupon_selected, admin_coupons.size());
    }
    void refresh() { refreshProducts(); refreshCart(); refreshUser(); refreshSquare(); refreshAdmin(); refreshCheckout(); }
    Component loginPage();
    Component mallPage();
    Component cartPage();
    Component checkoutPage();
    Component userPage();
    Component squarePage();
    Component adminUsers();
    Component adminProducts();
    Component adminCoupons();
    Component adminSystem();
    Component adminPage();
};

Component Application::Impl::loginPage() {
    auto mode = Toggle(&login_modes, &login_mode);
    auto name = input(username, "用户名");
    auto pass = input(password, "密码", true);
    auto confirm = input(repeated_password, "再次输入密码", true);
    auto confirm_field = Renderer(confirm, [confirm] { return row("确认密码", confirm); });
    auto optional_confirm = Maybe(confirm_field, [this] { return login_mode == 1; });
    auto submit = button("确认 / 登录", [this] {
        if (login_mode == 1) {
            if (password != repeated_password) { notify(core::Result::fail("两次输入的密码不一致")); return; }
            auto result = ctx.registerCustomer(username, password); notify(result);
            if (!result.success) return;
        }
        const auto result = ctx.login(username, password, login_mode == 2); notify(result);
        if (result.success) {
            password.clear(); repeated_password.clear();
            navigate(login_mode == 2 ? Page::Admin : Page::Mall);
        }
    });
    auto quit = button("退出", [this] { requestQuit(); });
    auto actions = Container::Horizontal({submit, quit});
    auto controls = Container::Vertical({mode, name, pass, optional_confirm, actions});
    focus_order[static_cast<int>(Page::Login)] = {mode, name, pass, optional_confirm, submit, quit};
    return Renderer(controls, [this, mode, name, pass, optional_confirm, actions] {
        return panel("欢迎回来", {text("挑选商品，比较优惠，让每一笔消费看得明白。") | dim,
            separator(), mode->Render(), row("用户名", name), row("密码", pass), optional_confirm->Render(),
            actions->Render(), separator(), text("演示消费者  demo / 123456"),
            text("运营  staff / staff123    超管  root / admin123"),
            paragraph("新注册账号按现有业务规则获赠1000元模拟余额；这里只模拟充值和支付。") | dim,
            paragraph("数据目录：" + ctx.storage_mgr.getDataDir().string()) | dim}) | size(WIDTH, LESS_THAN, 84) | hcenter;
    });
}

Component Application::Impl::mallPage() {
    auto filter_option = MenuOption::Toggle();
    filter_option.on_change = [this] { product_selected = 0; refreshProducts(); };
    auto filter = Menu(&categories, &category, filter_option);
    auto search_option = InputOption{};
    search_option.multiline = false;
    search_option.on_change = [this] { product_selected = 0; refreshProducts(); };
    auto query = Input(&search, "按商品名或编号搜索", search_option);
    auto list = Menu(&product_entries, &product_selected);
    auto amount = input(quantity, "数量");
    auto add = button("加入购物车", [this] {
        const auto p = chosen(products, product_selected); int value = 0;
        if (!p) { notify(core::Result::fail("暂无可选商品")); return; }
        if (!parseQuantity(quantity, value)) { notify(core::Result::fail("请输入1至1000000的整数数量")); return; }
        notify(ctx.addToCart(p->getId(), value)); refreshCart();
    });
    auto controls = Container::Vertical({filter, query, list, amount, add});
    focus_order[static_cast<int>(Page::Mall)] = {filter, query, list, amount, add};
    return Renderer(controls, [this, filter, query, list, amount, add] {
        const auto p = chosen(products, product_selected);
        Elements details = {text("选择左侧商品查看详情") | dim};
        if (p) details = {text(p->getName()) | bold, text("编号：" + p->getId()), text("品类：" + p->getCategoryName()),
            text("单价：" + money(p->getPrice())) | color(accent), text("可售库存：" + std::to_string(p->getStock())),
            text("购物车已有：" + std::to_string(ctx.cart.getItemQuantity(p->getId()))), row("加购数量", amount), add->Render()};
        return vbox({text("商城 / MALL") | bold, filter->Render(), row("搜索", query),
            columns(panel("商品目录", {products.empty() ? text("没有匹配的商品") : listView(list, 12)}), panel("商品详情", details)),
            text("购物车 " + std::to_string(ctx.cart.getTotalQuantity()) + " 件 · 原价合计 " + money(ctx.cart.getTotalOriginalAmount()))});
    });
}

Component Application::Impl::cartPage() {
    auto option = MenuOption::Vertical(); option.on_change = [this] { syncCartQuantity(); };
    auto list = Menu(&cart_entries, &cart_selected, option);
    auto amount = input(cart_quantity, "数量，0表示移除");
    auto apply = button("设置数量", [this] {
        int value = 0;
        if (!parseQuantity(cart_quantity, value, true)) { notify(core::Result::fail("数量需为0至1000000的整数")); return; }
        notify(ctx.setCartQuantity(selectedCartId(), value)); refreshCart();
    });
    auto decrease = button("−1", [this] { const auto id = selectedCartId(); notify(ctx.setCartQuantity(id, ctx.cart.getItemQuantity(id) - 1)); refreshCart(); });
    auto increase = button("+1", [this] { const auto id = selectedCartId(); notify(ctx.setCartQuantity(id, ctx.cart.getItemQuantity(id) + 1)); refreshCart(); });
    auto remove = button("移除", [this] { notify(ctx.setCartQuantity(selectedCartId(), 0)); refreshCart(); });
    auto clear = button("清空", [this] { ctx.cart.clear(); refreshCart(); notify(core::Result::ok("购物车已清空")); });
    auto checkout = button("去结算", [this] { navigate(Page::Checkout); });
    auto back_to_mall = button("继续选购", [this] { navigate(Page::Mall); });
    auto actions = Container::Horizontal({decrease, increase, remove, clear});
    auto next = Container::Horizontal({checkout, back_to_mall});
    auto controls = Container::Vertical({list, amount, apply, actions, next});
    focus_order[static_cast<int>(Page::Cart)] = {list, amount, apply, decrease, increase, remove, clear, checkout, back_to_mall};
    return Renderer(controls, [this, list, amount, apply, actions, next] {
        return panel("购物车 / CART", {cart_items.empty() ? text("购物车还是空的，去商城选购吧。") : listView(list, 10),
            separator(), row("目标数量", amount), apply->Render(), actions->Render(),
            text("共 " + std::to_string(ctx.cart.getTotalQuantity()) + " 件 · 原价 " + money(ctx.cart.getTotalOriginalAmount())) | bold,
            text("结算页会分别展示会员优惠、券优惠和最终实付。") | dim, next->Render()});
    });
}

Component Application::Impl::checkoutPage() {
    auto option = MenuOption::Vertical(); option.on_change = [this] { updateQuote(); };
    auto list = Menu(&checkout_entries, &checkout_selected, option);
    auto pay = button("确认支付", [this] {
        const auto result = ctx.pay(preview); notify(result);
        if (result.success) navigate(Page::Mall); else refreshCheckout();
    });
    auto back_to_cart = button("返回购物车", [this] { navigate(Page::Cart); });
    auto actions = Container::Horizontal({pay, back_to_cart});
    auto controls = Container::Vertical({list, actions});
    focus_order[static_cast<int>(Page::Checkout)] = {list, pay, back_to_cart};
    return Renderer(controls, [this, list, actions] {
        const auto& bill = preview.plan;
        Elements details;
        if (checkout_selected >= 2 && checkout_selected - 2 < static_cast<int>(evaluated.size())) {
            const auto& entry = evaluated[checkout_selected - 2]; details = couponDetails(entry.coupon);
            std::string diagnostic = entry.check.is_eligible ? "当前订单可用" : entry.check.fail_reason;
            if (entry.check.shortfall_amount > 0) diagnostic += "，还差 " + money(entry.check.shortfall_amount);
            details.push_back(paragraph(diagnostic) | color(entry.check.is_eligible ? Color::GreenLight : Color::RedLight));
        } else details = {paragraph(checkout_selected == 0 ? "自动比较你持有的全部优惠券，选出当前实付最低的单券方案。" : "本单仅享受会员等级优惠。")};
        const auto c = ctx.user_mgr.getCurrentCustomer();
        return vbox({text("结算 / CHECKOUT") | bold,
            columns(panel("选择优惠券", {listView(list, 8), separator(), vbox(details)}),
                panel("本单账单", {text("商品原价    " + money(bill.origin_total)),
                    text("会员优惠  − " + money(bill.member_discount_saving)), text("优惠券    − " + money(bill.coupon_discount_saving)),
                    separator(), text("最终实付    " + money(bill.final_pay_amount)) | bold | color(accent),
                    paragraph("使用：" + (bill.best_coupon ? bill.best_coupon->getName() : "不使用优惠券")),
                    text("账户余额    " + money(c ? c->getBalance() : 0)), text("单券使用 · 会员与券平行让利") | dim})), actions->Render()});
    });
}

Component Application::Impl::userPage() {
    auto amount = input(recharge_amount, "充值金额，如100.00");
    auto recharge = button("模拟充值", [this] {
        double value = 0;
        if (!parseAmount(recharge_amount, value)) { notify(core::Result::fail("金额需大于0，最多两位小数")); return; }
        notify(ctx.recharge(value)); refreshUser();
    });
    auto coupons = Menu(&wallet_entries, &wallet_selected);
    auto transactions = Menu(&transaction_entries, &transaction_selected);
    auto controls = Container::Vertical({amount, recharge, coupons, transactions});
    focus_order[static_cast<int>(Page::UserCenter)] = {amount, recharge, coupons, transactions};
    return Renderer(controls, [this, amount, recharge, coupons, transactions] {
        const auto c = ctx.user_mgr.getCurrentCustomer();
        if (!c) return text("请登录后查看个人中心");
        auto info = panel("会员与余额", {text(c->getUsername() + " · " + c->getTierName()) | bold,
            text("当前余额 " + money(c->getBalance())) | color(accent), text("累计实付 " + money(c->getTotalSpent())),
            gauge(static_cast<float>(std::clamp(c->getGrowthProgress(), 0.0, 1.0))) | color(accent),
            text(c->isMaxTier() ? "已达到最高等级" : "距离下一级还需消费 " + money(c->getAmountNeededForNextTier())),
            row("充值金额", amount), recharge->Render()});
        auto wallet_info = couponDetails(chosen(wallet, wallet_selected));
        wallet_info.insert(wallet_info.begin(), wallet.empty() ? text("还没有券，前往领券中心领取。") : listView(coupons, 5));
        return vbox({text("个人中心 / ACCOUNT") | bold, columns(info, panel("我的卡券", wallet_info)),
            panel("全部账单 · 最新在前", {transaction_entries.empty() ? text("暂无流水") : listView(transactions, 7)})});
    });
}

Component Application::Impl::squarePage() {
    auto option = MenuOption::Toggle(); option.on_change = [this] { square_selected = 0; refreshSquare(); };
    auto filter = Menu(&square_filters, &square_filter, option);
    auto list = Menu(&square_entries, &square_selected);
    auto get = button("领取 / 购买所选券", [this] {
        const auto cp = chosen(square, square_selected);
        if (!cp) { notify(core::Result::fail("暂无可选优惠券")); return; }
        notify(ctx.acquireCoupon(cp->getTemplateId())); refreshUser();
    });
    auto controls = Container::Vertical({filter, list, get});
    focus_order[static_cast<int>(Page::CouponSquare)] = {filter, list, get};
    return Renderer(controls, [this, filter, list, get] {
        const auto cp = chosen(square, square_selected); auto details = couponDetails(cp);
        if (cp) {
            const auto c = ctx.user_mgr.getCurrentCustomer();
            if (c && c->getTier() < cp->getMinTier()) details.push_back(text("当前会员等级不满足领取要求") | color(Color::RedLight));
            details.push_back(get->Render());
            details.push_back(paragraph("付费券点击后将从余额扣除所示购券价格；时间和品类限制在结算时判定。") | dim);
        }
        return vbox({text("领券中心 / COUPONS") | bold, filter->Render(), columns(
            panel("公共券池", {square.empty() ? text("暂无此类优惠券") : listView(list, 11)}), panel("券规则", details))});
    });
}

Component Application::Impl::adminUsers() {
    auto list = Menu(&admin_user_entries, &admin_user_selected);
    auto tier = Toggle(&tiers, &new_tier);
    auto apply = button("修改所选用户等级", [this] {
        const auto c = chosen(admin_users, admin_user_selected);
        if (!c) { notify(core::Result::fail("请先选择用户")); return; }
        notify(ctx.changeTier(c->getUsername(), static_cast<core::UserTier>(new_tier))); refreshAdmin();
    });
    auto controls = Container::Vertical({list, tier, apply});
    admin_focus_order[0] = {list, tier, apply};
    return Renderer(controls, [this, list, tier, apply] {
        Elements details = {text("暂无用户")};
        if (const auto c = chosen(admin_users, admin_user_selected)) details = {
            text(c->getUsername()) | bold, text("等级：" + c->getTierName()), text("累计消费：" + money(c->getTotalSpent())),
            text("余额：" + money(c->getBalance())), text("卡券：" + std::to_string(c->getCouponCount()) + " 张"),
            text("目标等级（仅超级管理员可修改）"), tier->Render(), apply->Render()};
        return columns(panel("消费者列表", {admin_users.empty() ? text("暂无消费者") : listView(list, 12)}), panel("用户详情", details));
    });
}

Component Application::Impl::adminProducts() {
    auto list = Menu(&admin_product_entries, &admin_product_selected);
    auto count = input(restock_count, "补货件数");
    auto restock = button("为所选商品补货", [this] {
        int value = 0; const auto p = chosen(admin_products, admin_product_selected);
        if (!p || !parseQuantity(restock_count, value)) { notify(core::Result::fail("请选择商品并填写正整数补货数量")); return; }
        notify(ctx.replenish(p->getId(), value)); refreshAdmin();
    });
    auto id = input(product_id, "唯一编号，如P6001"); auto name = input(product_name, "商品名称");
    auto cat = Toggle(&product_categories, &new_category); auto price = input(product_price, "如49.90");
    auto stock = input(product_stock, "初始库存");
    auto add = button("上架新商品", [this] {
        double price_value = 0; int stock_value = 0;
        if (!parseAmount(product_price, price_value) || !parseQuantity(product_stock, stock_value, true)) {
            notify(core::Result::fail("价格需大于0（最多两位小数），库存需为非负整数")); return;
        }
        const auto result = ctx.addProduct(product_id, product_name, static_cast<core::ProductCategory>(new_category + 1), price_value, stock_value);
        notify(result);
        if (result.success) { product_id.clear(); product_name.clear(); product_price.clear(); }
        refreshAdmin();
    });
    auto controls = Container::Vertical({list, count, restock, id, name, cat, price, stock, add});
    admin_focus_order[1] = {list, count, restock, id, name, cat, price, stock, add};
    return Renderer(controls, [this, list, count, restock, id, name, cat, price, stock, add] {
        return columns(panel("库存管理", {admin_products.empty() ? text("暂无商品") : listView(list, 9), row("补货数量", count), restock->Render()}),
            panel("上架新商品", {row("商品编号", id), row("商品名称", name), cat->Render(), row("单价 / 元", price), row("初始库存", stock), add->Render()}));
    });
}

Component Application::Impl::adminCoupons() {
    auto list = Menu(&admin_coupon_entries, &admin_coupon_selected);
    auto id = input(coupon_id, "唯一模板编号"); auto name = input(coupon_name, "优惠券名称");
    auto type = Toggle(&coupon_types, &coupon_type); auto cost = input(coupon_cost, "0为免费");
    auto minimum = input(coupon_minimum, "原价门槛"); auto value = input(coupon_value, "金额或折扣比例");
    auto cap = input(coupon_cap, "0为不封顶"); auto cat = Toggle(&categories, &coupon_category);
    auto tier = Toggle(&tiers, &coupon_tier); auto time = Toggle(&times, &coupon_time);
    auto minimum_field = Renderer(minimum, [minimum] { return row("满额门槛", minimum); });
    auto optional_minimum = Maybe(minimum_field, [this] { return coupon_type != 2; });
    auto cap_field = Renderer(cap, [cap] { return row("减免上限", cap); });
    auto optional_cap = Maybe(cap_field, [this] { return coupon_type == 1; });
    auto publish = button("发布新券", [this] {
        CouponDraft draft;
        draft.id = coupon_id; draft.name = coupon_name; draft.type = coupon_type;
        draft.category = static_cast<core::ProductCategory>(coupon_category);
        draft.tier = static_cast<core::UserTier>(coupon_tier); draft.time = static_cast<core::TimeConstraint>(coupon_time);
        if (!parseAmount(coupon_cost, draft.cost, true) ||
            (coupon_type != 2 && !parseAmount(coupon_minimum, draft.minimum, true)) ||
            !parseAmount(coupon_value, draft.value) ||
            (coupon_type == 1 && !parseAmount(coupon_cap, draft.cap, true))) {
            notify(core::Result::fail("请填写有效的券参数，数值最多两位小数")); return;
        }
        const auto result = ctx.issueCoupon(draft); notify(result);
        if (result.success) { coupon_id.clear(); coupon_name.clear(); } refreshAdmin();
    });
    auto controls = Container::Vertical({list, id, name, type, cost, optional_minimum, value, optional_cap, cat, tier, time, publish});
    admin_focus_order[2] = {list, id, name, type, cost, optional_minimum, value, optional_cap, cat, tier, time, publish};
    return Renderer(controls, [this, list, id, name, type, cost, optional_minimum, value, optional_cap, cat, tier, time, publish] {
        auto details = couponDetails(chosen(admin_coupons, admin_coupon_selected));
        details.insert(details.begin(), admin_coupons.empty() ? text("暂无优惠券") : listView(list, 5));
        return columns(panel("已有券模板", details), panel("发布新券", {
            row("模板编号", id), row("券名称", name), type->Render(), row("购券费用", cost), optional_minimum->Render(),
            row(coupon_type == 1 ? "折扣比例" : "减免金额", value), optional_cap->Render(),
            text(coupon_type == 1 ? "0.85 = 85折；上限0 = 不封顶" : "立减券没有满额门槛") | dim,
            row("适用品类", cat), row("最低等级", tier), row("生效时间", time), publish->Render()}));
    });
}

Component Application::Impl::adminSystem() {
    auto confirm = input(reset_confirmation, "输入大写 RESET");
    auto reset_button = button("确认重置系统", [this] {
        const auto result = ctx.reset(reset_confirmation); notify(result);
        if (result.success) { reset_confirmation.clear(); navigate(Page::Login); }
    });
    auto controls = Container::Vertical({confirm, reset_button});
    admin_focus_order[3] = {confirm, reset_button};
    return Renderer(controls, [this, confirm, reset_button] {
        return panel("系统设置", {paragraph("数据目录：" + ctx.storage_mgr.getDataDir().string()),
            text("每次业务变更后自动保存；购物车仅在本次登录期间保留。"), separator(),
            text("恢复演示数据（仅超级管理员）") | bold | color(Color::RedLight),
            paragraph("此操作会替换用户、余额、账单、商品和券池。已有数据将被清除，并退出当前登录。"),
            row("确认文字", confirm), reset_button->Render()});
    });
}

Component Application::Impl::adminPage() {
    auto sections = Container::Tab({adminUsers(), adminProducts(), adminCoupons(), adminSystem()}, &admin_tab);
    auto mode = Toggle(&admin_tabs, &admin_tab);
    auto controls = Container::Vertical({mode, sections});
    focus_order[static_cast<int>(Page::Admin)] = {mode};
    return Renderer(controls, [this, mode, sections] {
        const auto admin = ctx.user_mgr.getCurrentAdmin();
        if (!admin) return text("请登录管理员账号");
        return vbox({text("运营控制台 / ADMIN · " + admin->getRoleName()) | bold, mode->Render(), sections->Render()});
    });
}

Application::Application(AppContext& context, std::function<void()> exit)
    : impl_(std::make_unique<Impl>(context, std::move(exit))) {}
Application::~Application() = default;
Component Application::component() const { return impl_->root; }
Page Application::page() const { return static_cast<Page>(impl_->active); }
void Application::navigate(Page page) { impl_->navigate(page); }
} // namespace ui
