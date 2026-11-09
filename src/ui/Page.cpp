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
                           ButtonKind inactive_kind = ButtonKind::Tab) {
    return StyledButton(
        std::move(label_fn),
        std::move(on_click),
        [is_active_tab, inactive_kind] { return is_active_tab ? ButtonKind::TabActive : inactive_kind; });
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
    auto can_go_back = [this] {
        return ctx.stack_depth > 1 || !ctx.prev_page_name.empty();
    };

    auto btn_back_raw = StyledButton(
        [] { return "◀"; },
        [this] { pending_action = PageAction::pop(); },
        [] { return ButtonKind::Primary; });
    auto btn_back = Maybe(btn_back_raw, can_go_back);

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
        btn_back, btn_mall, btn_cart, btn_coupon, btn_user, btn_day, btn_logout
    });

    return Renderer(row, [=, this] {
        const bool show_back = can_go_back();
        Element left_back = show_back ? btn_back->Render() : text("   ");
        Element center_tabs = hbox({
            btn_mall->Render(),
            text("  "),
            btn_cart->Render(),
            text("  "),
            btn_coupon->Render(),
            text("  "),
            btn_user->Render()
        });
        return hbox({
            std::move(left_back),
            filler(),
            std::move(center_tabs),
            filler(),
            btn_day->Render(),
            text(" "),
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
    }), nav_bar->Focused(), PanelStyle::Header);
}

Element Page::renderFooterBar(const std::string& key_hints) const {
    const bool has_msg = !status_msg.empty();
    const Element msg_line = has_msg
        ? hbox({text(" ✓ ") | bold | color(theme::Good), text(status_msg) | color(theme::Good) | flex})
        : hbox({text(" 就绪") | color(theme::Muted)});
    const Element hint_line = hbox({text(" " + key_hints) | color(theme::Muted), filler(),
        text("栈 " + std::to_string(ctx.stack_depth) + "/8 ") | color(theme::Muted)});

    return vbox({msg_line, hint_line}) | borderStyled(ROUNDED, theme::Muted);
}

void Page::showPopup(std::string title, std::string msg, bool err) {
    popup_ = Popup{};
    popup_.open = true;
    popup_.error = err;
    popup_.title = std::move(title);
    popup_.msg = std::move(msg);

    // 推荐导航：仅消费者会话 + 错误提示
    if (err && ctx.user_mgr.getCurrentCustomer()) {
        const std::string& m = popup_.msg;
        auto has = [&m](const char* kw) { return m.find(kw) != std::string::npos; };
        if (has("余额不足")) {
            popup_.navs.push_back({"去个人中心充值", [this] { return std::make_unique<UserCenterPage>(ctx); }});
        } else if (has("购物车为空")) {
            popup_.navs.push_back({"去商城选购", [this] { return std::make_unique<MallPage>(ctx); }});
        } else if (has("库存")) {
            popup_.navs.push_back({"返回商城", [this] { return std::make_unique<MallPage>(ctx); }});
            popup_.navs.push_back({"查看购物车", [this] { return std::make_unique<CartPage>(ctx); }});
        } else if (has("等级") || has("优惠券") || has("券")) {
            popup_.navs.push_back({"去领券中心", [this] { return std::make_unique<CouponSquarePage>(ctx); }});
        }
    }
    popup_.cursor = static_cast<int>(popup_.navs.size()); // 默认聚焦"确定"，防止 Enter 连击误跳转
}

void Page::handlePopupEvent(Event event, PageAction& out_action) {
    const int last = static_cast<int>(popup_.navs.size()); // "确定"的下标
    auto activate = [&](int idx) {
        popup_.open = false;
        if (idx < last) out_action = PageAction::push(popup_.navs[static_cast<std::size_t>(idx)].make());
    };
    if (event.is_mouse()) {
        const auto m = event.mouse();
        for (int i = 0; i <= last && i < static_cast<int>(popup_.boxes.size()); ++i) {
            if (!popup_.boxes[static_cast<std::size_t>(i)].Contain(m.x, m.y)) continue;
            popup_.cursor = i; // 悬停即移动光标
            if (m.button == Mouse::Left && m.motion == Mouse::Pressed) activate(i);
            break;
        }
        return;
    }
    if (event == Event::Escape) {
        popup_.open = false;
    } else if (event == Event::ArrowLeft || event == Event::TabReverse) {
        popup_.cursor = (popup_.cursor + last) % (last + 1);
    } else if (event == Event::ArrowRight || event == Event::Tab) {
        popup_.cursor = (popup_.cursor + 1) % (last + 1);
    } else if (event == Event::Return || event == Event::Character(' ')) {
        activate(popup_.cursor);
    }
    // 其余按键（含鼠标）一律吞掉，由 PageManager 保证不下传
}

Element Page::renderPopupBox() const {
    const Color tone = popup_.error ? theme::Bad : theme::Good;
    const std::string icon = popup_.error ? " ✗ " : " ✓ ";

    Elements buttons;
    popup_.boxes.resize(popup_.navs.size() + 1);
    for (std::size_t i = 0; i <= popup_.navs.size(); ++i) {
        const bool is_ok = (i == popup_.navs.size());
        const bool focused = static_cast<int>(i) == popup_.cursor;
        Element b = text(" " + (is_ok ? std::string("确定") : popup_.navs[i].label) + " ");
        if (focused) b = b | bold | color(Color::Black) | bgcolor(is_ok ? tone : theme::Accent);
        else b = b | color(is_ok ? Color::GrayLight : theme::Accent);
        if (!buttons.empty()) buttons.push_back(text("  "));
        buttons.push_back(std::move(b) | reflect(popup_.boxes[i]));
    }

    return vbox({
               text(icon + popup_.title + " ") | bold | color(tone) | hcenter,
               separator() | color(Color::GrayDark),
               paragraph(popup_.msg) | color(Color::White) | xflex,
               text(""),
               hbox(std::move(buttons)) | hcenter,
               text(""),
               text("←/→ 切换 · Enter 确认 · Esc 关闭") | color(theme::Muted) | hcenter,
           }) |
           size(WIDTH, GREATER_THAN, 36) | size(WIDTH, LESS_THAN, 56) |
           borderStyled(ROUNDED, tone) | clear_under | center;
}

Element Page::renderWithPopup() {
    Element base = render();
    if (!popup_.open) return base;
    return dbox({std::move(base) | dim, renderPopupBox()});
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
