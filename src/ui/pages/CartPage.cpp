#include "ui/pages/CartPage.hpp"
#include "ui/pages/CheckoutPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/MallPage.hpp"

#include <algorithm>

namespace ui {

using namespace ftxui;

CartPage::CartPage(AppContext& context)
    : Page(context, "按 ↑/↓ 移动光标，Enter 选中商品；可按 +/- 调数量、Del 移除，或按 → 进入右侧操作区。") {
    nav_bar_ = buildCustomerNavBar(2);

    cart_list_ = DataList(
        &selected_idx_,
        [this] { return static_cast<int>(items_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& it = items_[static_cast<std::size_t>(idx)];
            return hbox({
                text(" "),
                col(std::to_string(idx + 1), 4) | color(theme::Muted),
                col(it.getProductName(), 22),
                col(money(it.getProductPrice()), 12),
                col("x" + std::to_string(it.quantity), 8) | color(theme::Warn),
                text(money(it.getSubtotal())) | bold | color(theme::Good)
            });
        },
        [this] {
            syncItems();
            if (!items_.empty()) {
                const auto& sel = items_[static_cast<std::size_t>(selected_idx_)];
                custom_qty_ = std::to_string(sel.quantity);
                setStatus("已选中「" + sel.getProductName() + "」，按 +/- 调整数量、Del 移除，或按 → 进入右侧面板。", false);
            }
        },
        "购物车当前为空。请点击右侧「继续选购商品」 或按 Esc 返回商城。");

    dec_btn_      = ActionButton("-", [this] { modifySelectedQty(-1); });
    inc_btn_      = ActionButton("+", [this] { modifySelectedQty(1); });
    qty_input_    = TextInput(&custom_qty_, "数量", false, [this] { applyCustomQty(); });
    set_qty_btn_  = ActionButton("设定", [this] { applyCustomQty(); }, true);

    remove_btn_   = DangerButton("移除选中项(Del)", [this] { removeSelected(); });
    clear_btn_    = DangerButton("清空购物车(C)", [this] { clearAll(); });

    checkout_btn_ = BigActionButton("前往收银台算券结算", [this] { enterCheckout(); }, ButtonKind::Primary);
    go_coupon_btn_ = ActionButton("去领券中心", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    });
    go_mall_btn_ = ActionButton("继续选购商品", [this] {
        pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
    });

    auto ops_row1 = Container::Horizontal({dec_btn_, inc_btn_, qty_input_, set_qty_btn_});
    auto ops_row2 = Container::Horizontal({remove_btn_, clear_btn_});
    auto nav_row  = Container::Horizontal({go_coupon_btn_, go_mall_btn_});
    auto right_controls = Container::Vertical({ops_row1, ops_row2, checkout_btn_, nav_row});

    container = Container::Vertical({
        nav_bar_,
        Container::Horizontal({cart_list_, right_controls})
    });

    syncItems();
    if (!items_.empty()) {
        custom_qty_ = std::to_string(items_[static_cast<std::size_t>(selected_idx_)].quantity);
    }
    cart_list_->TakeFocus();
}

void CartPage::on_enter() {
    syncItems();
    if (!items_.empty()) {
        custom_qty_ = std::to_string(items_[static_cast<std::size_t>(selected_idx_)].quantity);
    }
    cart_list_->TakeFocus();
}

void CartPage::syncItems() {
    items_ = ctx.cart.getItemslist();
    if (items_.empty()) {
        selected_idx_ = 0;
    } else {
        selected_idx_ = std::clamp(selected_idx_, 0, static_cast<int>(items_.size()) - 1);
    }
}

void CartPage::modifySelectedQty(int delta) {
    syncItems();
    if (items_.empty()) {
        setStatus("购物车为空", true);
        return;
    }
    const auto id = items_[static_cast<std::size_t>(selected_idx_)].getProductId();
    notify(ctx.changeCartQty(id, delta));
    syncItems();
    if (!items_.empty()) {
        custom_qty_ = std::to_string(items_[static_cast<std::size_t>(selected_idx_)].quantity);
    }
}

void CartPage::applyCustomQty() {
    syncItems();
    if (items_.empty()) {
        setStatus("购物车为空，无法设定数量", true);
        return;
    }
    int qty = 0;
    if (!parseQuantity(custom_qty_, qty, true)) {
        setStatus("请输入非负整数数量（输入 0 将移除该商品）", true);
        return;
    }
    const auto id = items_[static_cast<std::size_t>(selected_idx_)].getProductId();
    notify(ctx.setCartQty(id, qty));
    syncItems();
    if (!items_.empty()) {
        custom_qty_ = std::to_string(items_[static_cast<std::size_t>(selected_idx_)].quantity);
    }
}

void CartPage::removeSelected() {
    syncItems();
    if (items_.empty()) {
        setStatus("购物车为空", true);
        return;
    }
    const auto id = items_[static_cast<std::size_t>(selected_idx_)].getProductId();
    notify(ctx.removeCartItem(id));
    syncItems();
    if (!items_.empty()) {
        custom_qty_ = std::to_string(items_[static_cast<std::size_t>(selected_idx_)].quantity);
    }
}

void CartPage::clearAll() {
    notify(ctx.clearCart());
    syncItems();
}

void CartPage::enterCheckout() {
    if (ctx.cart.empty()) {
        setStatus("购物车为空，请先去商城添加商品后再结算。", true);
        return;
    }
    pending_action = PageAction::push(std::make_unique<CheckoutPage>(ctx));
}

bool CartPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (event == Event::Character('+') || event == Event::Character('=')) {
            modifySelectedQty(1);
            handled = true;
        } else if (event == Event::Character('-') || event == Event::Character('_')) {
            modifySelectedQty(-1);
            handled = true;
        } else if (event == Event::Delete || event == Event::Backspace ||
                   event == Event::Character('d') || event == Event::Character('D')) {
            removeSelected();
            handled = true;
        } else if (event == Event::Character('c') || event == Event::Character('C')) {
            clearAll();
            handled = true;
        } else if (handleCustomerShortcuts(event, 2)) {
            handled = true;
        } else if (event == Event::Escape) {
            pending_action = PageAction::pop();
            handled = true;
        }
    }
    out_action = std::move(pending_action);
    pending_action = PageAction::none();
    return handled || (out_action.type != PageAction::Type::None);
}

Element CartPage::render() {
    syncItems();

    Element table_header = hbox({
        text("  "),
        col("#", 4),
        col("商品名称", 22),
        col("单价", 12),
        col("数量", 8),
        text("小计")
    }) | color(theme::Muted);

    Element left_box = panel(
        "购物车商品明细",
        vbox({
            std::move(table_header),
            separator() | color(theme::Muted),
            cart_list_->Render() | flex
        }),
        cart_list_->Focused(),
        PanelStyle::Primary);

    const bool right_focused = inc_btn_->Focused() || dec_btn_->Focused() ||
                               qty_input_->Focused() || set_qty_btn_->Focused() ||
                               remove_btn_->Focused() || clear_btn_->Focused() ||
                               checkout_btn_->Focused() || go_coupon_btn_->Focused() ||
                               go_mall_btn_->Focused();

    Element sel_summary;
    int cur_sel_qty = 0;
    if (!items_.empty()) {
        const auto& sel = items_[static_cast<std::size_t>(selected_idx_)];
        cur_sel_qty = sel.quantity;
        sel_summary = vbox({
            kv(" 当前选中:", text(sel.getProductName()) | bold | color(theme::Accent)),
            kv(" 商品单价:", text(money(sel.getProductPrice())) | color(Color::White)),
            kv(" 数量 / 小计:",
               hbox({
                   text("x" + std::to_string(sel.quantity) + " 件") | bold | color(theme::Warn),
                   text("  =  "),
                   text(money(sel.getSubtotal())) | bold | color(theme::Good)
               }))
        });
    } else {
        sel_summary = text(" 当前未选中商品（购物车为空）") | color(theme::Muted);
    }

    Element item_ops_box = vbox({
        sectionHeader("选中商品操控", theme::Accent),
        std::move(sel_summary),
        hbox({
            text(" 数量步进: ") | color(theme::Muted),
            dec_btn_->Render(),
            text(" " + std::to_string(cur_sel_qty) + " ") | bold | color(Color::White),
            inc_btn_->Render(),
            filler(),
            text("直设:") | color(theme::Muted),
            qty_input_->Render() | size(WIDTH, EQUAL, 6),
            text(" "),
            set_qty_btn_->Render()
        }),
        hbox({
            remove_btn_->Render(),
            filler(),
            clear_btn_->Render()
        })
    });

    double total_orig = 0.0;
    int total_qty = 0;
    double cat_subs[6] = {0.0};
    for (const auto& it : items_) {
        const double sub = it.getSubtotal();
        total_orig += sub;
        total_qty += it.quantity;
        const int cat_idx = static_cast<int>(it.getProductCategory());
        if (cat_idx >= 1 && cat_idx <= 5) {
            cat_subs[cat_idx] += sub;
        }
    }

    Elements cat_rows;
    const core::ProductCategory cats[] = {
        core::ProductCategory::DIGITAL,
        core::ProductCategory::CLOTHING,
        core::ProductCategory::FOOD,
        core::ProductCategory::DAILY,
        core::ProductCategory::OTHER
    };
    for (auto cat : cats) {
        const double sub = cat_subs[static_cast<int>(cat)];
        if (sub > 1e-4) {
            cat_rows.push_back(kv(" · " + core::getCategoryName(cat) + "小计:", money(sub)));
        }
    }
    if (cat_rows.empty()) {
        cat_rows.push_back(text(" 暂无加购商品") | color(theme::Muted));
    }

    const auto cust = ctx.user_mgr.getCurrentCustomer();
    const auto tier = cust ? cust->getTier() : core::UserTier::V0_NORMAL;
    const auto tcfg = core::getTierConfig(tier);
    const double est_member_pay = total_orig * tcfg.discount_rate;

    const std::string summary_label = " 原价总额 (" +
        std::to_string(items_.size()) + "种/" +
        std::to_string(total_qty) + "件):";

    Element right_box = panel(
        "商品操控与金额汇总",
        vbox({
            std::move(item_ops_box),
            separator() | color(theme::Muted),
            sectionHeader("分品类金额统计", theme::Good),
            vbox(std::move(cat_rows)),
            kv(summary_label, text(money(total_orig)) | bold | color(theme::Good)),
            kv(" 会员折后预估 (" + tcfg.name + "):",
               text(money(est_member_pay)) | bold | color(theme::Warn)),
            filler(),
            separator() | color(theme::Muted),
            checkout_btn_->Render(),
            hbox({
                go_coupon_btn_->Render(),
                filler(),
                go_mall_btn_->Render()
            })
        }) | flex,
        right_focused,
        PanelStyle::Action) | size(WIDTH, EQUAL, 48);

    return renderCustomerFrame(
        nav_bar_,
        hbox({std::move(left_box) | flex, std::move(right_box)}),
        "↑/↓ 光标 | Enter 选中 | +/- 增减 | Del/D 移除选中项 | C 清空全部 | Tab/→ 进右侧操作区 | Esc 返回");
}

} // namespace ui
