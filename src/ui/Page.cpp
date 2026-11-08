#include "ui/Page.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/LoginPage.hpp"
#include "ui/pages/MallPage.hpp"
#include "ui/pages/UserCenterPage.hpp"

#include <iomanip>
#include <sstream>

namespace ui {

using namespace ftxui;

namespace {

Component makeNavTabButton(std::function<std::string()> label_fn,
                           bool is_active_tab,
                           std::function<void()> on_click,
                           Color custom_color = Color::GrayLight) {
    ButtonOption opt;
    opt.on_click = std::move(on_click);
    opt.transform = [label_fn = std::move(label_fn), is_active_tab, custom_color](const EntryState& st) -> Element {
        const std::string lbl = label_fn();
        Element el = text(" [ " + lbl + " ] ");
        if (st.focused) {
            return std::move(el) | bold | color(Color::Black) | bgcolor(Color::White);
        }
        if (is_active_tab) {
            return std::move(el) | bold | color(Color::White) | bgcolor(theme::RowSelectBg);
        }
        return std::move(el) | color(custom_color);
    };
    return Button(opt);
}

std::string tierCode(core::UserTier tier) {
    return "V" + std::to_string(static_cast<int>(tier));
}

std::string discountDesc(double rate) {
    if (rate >= 0.999) return "无基础折扣";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (rate * 10.0) << "折";
    return oss.str();
}

} // namespace

bool Page::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled && event == Event::Escape) {
        pending_action = PageAction::pop();
        handled = true;
    }
    out_action = std::move(pending_action);
    pending_action = PageAction::none();
    return handled || (out_action.type != PageAction::Type::None);
}

Component Page::buildCustomerNavBar(int active_tab) {
    auto btn_mall = makeNavTabButton(
        [] { return "1.商城货架"; },
        active_tab == 1,
        [this, active_tab] {
            if (active_tab != 1) pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
        });

    auto btn_cart = makeNavTabButton(
        [this] { return "2.购物车(" + std::to_string(ctx.cart.getTotalQuantity()) + ")"; },
        active_tab == 2,
        [this, active_tab] {
            if (active_tab != 2) pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
        });

    auto btn_coupon = makeNavTabButton(
        [] { return "3.领券中心"; },
        active_tab == 3,
        [this, active_tab] {
            if (active_tab != 3) pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
        });

    auto btn_user = makeNavTabButton(
        [] { return "4.个人中心"; },
        active_tab == 4,
        [this, active_tab] {
            if (active_tab != 4) pending_action = PageAction::push(std::make_unique<UserCenterPage>(ctx));
        });

    auto btn_back_raw = makeNavTabButton(
        [this] { return "◀ 返回:" + ctx.prev_page_name + "(Esc)"; },
        false,
        [this] { pending_action = PageAction::pop(); },
        theme::Accent);
    auto btn_back = Maybe(btn_back_raw, [this] { return !ctx.prev_page_name.empty(); });

    auto btn_day = makeNavTabButton(
        [] { return "切换工作日/周末(T)"; },
        false,
        [this] {
            ctx.toggleWeekendSimulation();
            setStatus("系统模拟时间已切换为：" + ctx.getDayLabel(), false);
        });

    auto btn_logout = makeNavTabButton(
        [] { return "退出登录"; },
        false,
        [this] {
            ctx.logout();
            pending_action = PageAction::reset(std::make_unique<LoginPage>(ctx));
        });

    auto row = Container::Horizontal({
        btn_mall, btn_cart, btn_coupon, btn_user, btn_back, btn_day, btn_logout
    });

    return Renderer(row, [=] {
        return hbox({
            text(" 页面跳转:") | color(theme::Muted),
            btn_mall->Render(),
            btn_cart->Render(),
            btn_coupon->Render(),
            btn_user->Render(),
            btn_back->Render(),
            filler(),
            btn_day->Render(),
            btn_logout->Render()
        });
    });
}

bool Page::handleCustomerShortcuts(const Event& event, int active_tab) {
    if (event == Event::Character('1')) {
        if (active_tab != 1) pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
        return true;
    }
    if (event == Event::Character('2')) {
        if (active_tab != 2) pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
        return true;
    }
    if (event == Event::Character('3')) {
        if (active_tab != 3) pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
        return true;
    }
    if (event == Event::Character('4')) {
        if (active_tab != 4) pending_action = PageAction::push(std::make_unique<UserCenterPage>(ctx));
        return true;
    }
    if (event == Event::Character('t') || event == Event::Character('T')) {
        ctx.toggleWeekendSimulation();
        setStatus("系统模拟时间已切换为：" + ctx.getDayLabel(), false);
        return true;
    }
    return false;
}

Element Page::renderCustomerHeader(const Component& nav_bar) const {
    const auto cust = ctx.user_mgr.getCurrentCustomer();
    const std::string uname = cust ? cust->getUsername() : "未登录";
    const core::UserTier tier = cust ? cust->getTier() : core::UserTier::V0_NORMAL;
    const auto tcfg = core::getTierConfig(tier);
    const std::string tier_str = tierCode(tier) + " " + tcfg.name + " (" + discountDesc(tcfg.discount_rate) + ")";
    const double bal = cust ? cust->getBalance() : 0.0;
    const std::size_t avail_cpns = cust ? cust->getAvailableCouponCount() : 0;

    Element info_row = hbox({
        text(" 当前用户: ") | color(theme::Muted),
        text(uname) | bold | color(Color::White),
        text("   会员等级: ") | color(theme::Muted),
        text(tier_str) | bold | color(theme::Warn),
        text("   账户余额: ") | color(theme::Muted),
        text(money(bal)) | bold | color(theme::Good),
        text("   可用卡券: ") | color(theme::Muted),
        text(std::to_string(avail_cpns) + " 张") | bold | color(theme::Accent),
        filler(),
        text("系统星期: ") | color(theme::Muted),
        text(ctx.getDayLabel() + " ") | bold | color(ctx.isWeekend() ? theme::Warn : Color::White)
    });

    return panel("系统状态与全局导航", vbox({
        std::move(info_row),
        separator() | color(theme::Muted),
        nav_bar->Render()
    }), nav_bar->Focused());
}

Element Page::renderFooterBar(const std::string& key_hints) const {
    const Color msg_color = is_error ? theme::Bad : theme::Good;
    const std::string prefix = is_error ? " [✗ 错误] " : " [✓ 提示] ";
    const std::string stack_info = " | 栈(" + std::to_string(ctx.stack_depth) + "/8) ";

    return hbox({
        text(prefix) | bold | color(msg_color),
        text(status_msg) | color(msg_color),
        filler(),
        text(key_hints + stack_info) | color(theme::Muted)
    }) | borderStyled(ROUNDED, theme::Muted);
}

Element Page::renderCustomerFrame(const Component& nav_bar,
                                  Element body,
                                  const std::string& key_hints) const {
    return vbox({
        renderCustomerHeader(nav_bar),
        std::move(body) | flex,
        renderFooterBar(key_hints)
    });
}

} // namespace ui
