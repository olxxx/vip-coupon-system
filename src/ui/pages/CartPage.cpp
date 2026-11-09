#include "ui/pages/CartPage.hpp"
#include "ui/pages/CheckoutPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/MallPage.hpp"

#include <algorithm>

namespace ui {

using namespace ftxui;

CartPage::CartPage(AppContext& context)
    : Page(context, "可在此调整商品数量 (+/-)，或点击「前往收银台」算券结算。") {
    nav_bar_ = buildCustomerNavBar(2);

    cart_list_ = DataList(
        &selected_idx_,
        [this] { return static_cast<int>(items_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& it = items_[static_cast<std::size_t>(idx)];
            return hbox({
                text(" "),
                col(std::to_string(idx + 1), 4) | color(theme::Muted),
                col(it.getProductId(), 8) | color(theme::Muted),
                col(it.getProductName(), 20),
                col(money(it.getProductPrice()), 11),
                col("x" + std::to_string(it.quantity), 8) | color(theme::Warn),
                text(money(it.getSubtotal())) | bold | color(theme::Good)
            });
        },
        {},
        "购物车当前为空。请点击右侧「继续选购商品」 或按 Esc 返回商城。");

    inc_btn_      = ActionButton("数量 +1", [this] { modifySelectedQty(1); });
    dec_btn_      = ActionButton("数量 -1", [this] { modifySelectedQty(-1); });
    remove_btn_   = ActionButton("移除该项", [this] { removeSelected(); });
    clear_btn_    = ActionButton("清空全部", [this] { clearAll(); });
    checkout_btn_ = ActionButton("前往收银台算券结算", [this] { enterCheckout(); }, true);
    go_coupon_btn_ = ActionButton("去领券中心", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    });
    go_mall_btn_ = ActionButton("继续选购商品", [this] {
        pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
    });

    auto ops_row1 = Container::Horizontal({inc_btn_, dec_btn_});
    auto ops_row2 = Container::Horizontal({remove_btn_, clear_btn_});
    auto nav_row  = Container::Horizontal({go_coupon_btn_, go_mall_btn_});
    auto right_controls = Container::Vertical({ops_row1, ops_row2, checkout_btn_, nav_row});

    container = Container::Vertical({
        nav_bar_,
        Container::Horizontal({cart_list_, right_controls})
    });

    syncItems();
    cart_list_->TakeFocus();
}

void CartPage::on_enter() {
    syncItems();
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
        col("编号", 8),
        col("商品名称", 20),
        col("单价", 11),
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
        cart_list_->Focused());

    const bool right_focused = inc_btn_->Focused() || dec_btn_->Focused() ||
                               remove_btn_->Focused() || clear_btn_->Focused() ||
                               checkout_btn_->Focused() || go_coupon_btn_->Focused() ||
                               go_mall_btn_->Focused();

    Elements cat_rows;
    const core::ProductCategory cats[] = {
        core::ProductCategory::DIGITAL,
        core::ProductCategory::CLOTHING,
        core::ProductCategory::FOOD,
        core::ProductCategory::DAILY,
        core::ProductCategory::OTHER
    };
    for (auto cat : cats) {
        const double sub = ctx.cart.getCategoryAmount(cat);
        if (sub > 1e-4) {
            cat_rows.push_back(kv(core::getCategoryName(cat) + "小计:", money(sub)));
        }
    }
    if (cat_rows.empty()) {
        cat_rows.push_back(text("暂无商品") | color(theme::Muted));
    }

    Element sel_info;
    if (!items_.empty()) {
        const auto& sel = items_[static_cast<std::size_t>(selected_idx_)];
        sel_info = vbox({
            hbox({
                text("当前选中: ") | color(theme::Muted),
                text(sel.getProductName()) | bold | color(Color::White),
                text(" (x" + std::to_string(sel.quantity) + ")") | color(theme::Warn)
            }),
            hbox({inc_btn_->Render(), dec_btn_->Render()}) | hcenter,
            hbox({remove_btn_->Render(), clear_btn_->Render()}) | hcenter
        });
    } else {
        sel_info = text("未选中商品项") | color(theme::Muted);
    }

    const std::string summary_label = "原价总额 (" +
        std::to_string(ctx.cart.getDistinctCount()) + "种/" +
        std::to_string(ctx.cart.getTotalQuantity()) + "件):";

    Element right_box = panel(
        "明细操作与金额统计",
        vbox({
            std::move(sel_info),
            separator() | color(theme::Muted),
            text("分品类金额统计:") | bold | color(Color::GrayLight),
            vbox(std::move(cat_rows)),
            separator() | color(theme::Muted),
            kv(summary_label,
               text(money(ctx.cart.getTotalOriginalAmount())) | bold | color(theme::Good)),
            filler(),
            separator() | color(theme::Muted),
            checkout_btn_->Render() | hcenter,
            hbox({go_coupon_btn_->Render(), go_mall_btn_->Render()}) | hcenter
        }) | flex,
        right_focused) | size(WIDTH, EQUAL, 44);

    return renderCustomerFrame(
        nav_bar_,
        hbox({std::move(left_box) | flex, std::move(right_box)}),
        "↑/↓ 选择条目 | +/- 增减数量 | Del 移除 | Enter 选中 | Tab 进入操作区 | Esc 返回");
}

} // namespace ui
