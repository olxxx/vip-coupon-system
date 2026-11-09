#include "ui/pages/MallPage.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"

#include <algorithm>

namespace ui {

using namespace ftxui;

MallPage::MallPage(AppContext& context, std::string welcome_msg)
    : Page(context,
           welcome_msg.empty()
               ? "当前焦点：商品货架列表。按 ↑/↓ 移动光标，Enter 选中商品，再点击「加入购物车」。"
               : std::move(welcome_msg)),
      cat_labels_({"全部", "数码3C", "服饰鞋包", "食品生鲜", "日用百货", "其他"}) {
    nav_bar_ = buildCustomerNavBar(1);

    cat_toggle_ = InlineToggle(&cat_labels_, &cat_filter_, [this] {
        selected_idx_ = 0;
        refreshFilter();
    });

    search_input_ = TextInput(
        &search_kw_,
        "商品名称/编号",
        false,
        [this] { product_list_->TakeFocus(); },
        [this] {
            selected_idx_ = 0;
            refreshFilter();
        });

    auto clear_raw = ActionButton("清空", [this] {
        search_kw_.clear();
        selected_idx_ = 0;
        refreshFilter();
    });
    clear_search_btn_ = Maybe(clear_raw, [this] { return !search_kw_.empty(); });

    product_list_ = DataList(
        &selected_idx_,
        [this] { return static_cast<int>(filtered_products_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& p = filtered_products_[static_cast<std::size_t>(idx)];
            Element stock_el;
            if (p->isOutOfStock()) {
                stock_el = text("售罄") | bold | color(theme::Bad);
            } else if (p->getStock() <= 5) {
                stock_el = text("仅剩" + std::to_string(p->getStock())) | bold | color(theme::Warn);
            } else {
                stock_el = text(std::to_string(p->getStock()));
            }
            return hbox({
                text("  "),
                col(p->getName(), 24),
                col(p->getCategoryName(), 12) | color(Color::GrayLight),
                col(p->getFormattedPrice(), 12) | color(theme::Good),
                std::move(stock_el)
            });
        },
        [this] { onProductSelected(); },
        "无匹配商品");

    qty_dec_btn_  = ActionButton("-", [this] { add_qty_ = std::max(1, add_qty_ - 1); });
    qty_inc_btn_  = ActionButton("+", [this] { add_qty_ = std::min(999, add_qty_ + 1); });
    add_cart_btn_ = BigActionButton("＋ 加入购物车", [this] { handleAddToCart(); }, ButtonKind::Primary);
    go_cart_btn_  = ActionButton("查看购物车", [this] {
        pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
    });
    go_coupon_btn_ = ActionButton("去领券中心", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    });

    auto filter_bar = Container::Horizontal({cat_toggle_, search_input_, clear_search_btn_});
    auto detail_controls = Container::Vertical({
        Container::Horizontal({qty_dec_btn_, qty_inc_btn_}),
        add_cart_btn_,
        Container::Horizontal({go_cart_btn_, go_coupon_btn_})
    });
    auto main_area = Container::Horizontal({product_list_, detail_controls});

    container = Container::Vertical({nav_bar_, filter_bar, main_area});
    refreshFilter();
    product_list_->TakeFocus();
}

void MallPage::on_enter() {
    refreshFilter();
    product_list_->TakeFocus();
}

void MallPage::refreshFilter() {
    const auto cat = static_cast<core::ProductCategory>(cat_filter_);
    const auto kw = trim(search_kw_);

    if (kw.empty()) {
        filtered_products_ = ctx.prod_mgr.getProductsByCategory(cat);
    } else {
        auto by_name = ctx.prod_mgr.searchByName(kw);
        auto by_cat = ctx.prod_mgr.getProductsByCategory(cat);
        filtered_products_.clear();
        for (const auto& p : by_cat) {
            if (!p) continue;
            const bool name_hit = std::any_of(by_name.begin(), by_name.end(),
                                              [&p](const auto& x) { return x && x->getId() == p->getId(); });
            const bool id_hit = (p->getId().find(kw) != std::string::npos);
            if (name_hit || id_hit) {
                filtered_products_.push_back(p);
            }
        }
    }

    if (filtered_products_.empty()) {
        selected_idx_ = 0;
    } else {
        selected_idx_ = std::clamp(selected_idx_, 0, static_cast<int>(filtered_products_.size()) - 1);
    }
}

void MallPage::onProductSelected() {
    refreshFilter();
    if (filtered_products_.empty()) return;
    const auto& sel = filtered_products_[static_cast<std::size_t>(selected_idx_)];
    setStatus("已选中 " + sel->getName() + "，按 Tab/→ 进入右侧详情区，点击「加入购物车」。", false);
}

void MallPage::handleAddToCart() {
    refreshFilter();
    if (filtered_products_.empty()) {
        setStatus("当前无可选商品", true);
        return;
    }
    const auto& sel = filtered_products_[static_cast<std::size_t>(selected_idx_)];
    notify(ctx.addToCart(sel->getId(), add_qty_));
}

bool MallPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (event == Event::Character('+') || event == Event::Character('=')) {
            add_qty_ = std::min(999, add_qty_ + 1);
            handled = true;
        } else if (event == Event::Character('-')) {
            add_qty_ = std::max(1, add_qty_ - 1);
            handled = true;
        } else if (event == Event::Character('/')) {
            search_input_->TakeFocus();
            handled = true;
        } else if (handleCustomerShortcuts(event, 1)) {
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

Element MallPage::render() {
    refreshFilter();
    std::shared_ptr<shop::Product> sel = nullptr;
    if (!filtered_products_.empty() && selected_idx_ >= 0 &&
        selected_idx_ < static_cast<int>(filtered_products_.size())) {
        sel = filtered_products_[static_cast<std::size_t>(selected_idx_)];
    }

    Element filter_box = panel("筛选条件", hbox({
        cat_toggle_->Render(),
        filler(),
        text("搜索: ") | color(theme::Muted),
        search_input_->Render() | size(WIDTH, EQUAL, 20),
        text(" "),
        clear_search_btn_->Render()
    }), cat_toggle_->Focused() || search_input_->Focused(), PanelStyle::SubNav);

    Element table_header = hbox({
        text("   "),
        col("商品名称", 24),
        col("分类", 12),
        col("单价", 12),
        text("库存")
    }) | color(theme::Muted);

    Element left_box = panel(
        "商品货架列表",
        vbox({
            std::move(table_header),
            separator() | color(theme::Muted),
            product_list_->Render() | flex
        }),
        product_list_->Focused(),
        PanelStyle::Primary);

    const bool right_focused = qty_dec_btn_->Focused() || qty_inc_btn_->Focused() ||
                               add_cart_btn_->Focused() || go_cart_btn_->Focused() ||
                               go_coupon_btn_->Focused();

    Element detail_content;
    if (sel) {
        const int in_cart = ctx.cart.getItemQuantity(sel->getId());
        const auto cust = ctx.user_mgr.getCurrentCustomer();
        const auto tier = cust ? cust->getTier() : core::UserTier::V0_NORMAL;
        const auto tcfg = core::getTierConfig(tier);
        const double member_price = sel->getPrice() * tcfg.discount_rate;
        const Color stock_color = sel->isOutOfStock()
            ? theme::Bad
            : (sel->getStock() <= 5 ? theme::Warn : Color::White);

        detail_content = vbox({
            kv(" 商品名称:", text(sel->getName()) | bold | color(Color::White)),
            kv(" 所属分类:", sel->getCategoryName()),
            kv(" 销售单价:", text(sel->getFormattedPrice()) | bold | color(theme::Good)),
            kv(" 会员折后:", text(money(member_price) + " (" + tcfg.name + ")")
                             | bold | color(theme::Warn)),
            kv(" 当前库存:", text(std::to_string(sel->getStock()) + " 件")
                             | bold | color(stock_color)),
            kv(" 购物车已加:", text(std::to_string(in_cart) + " 件")
                               | bold | color(in_cart > 0 ? theme::Accent : theme::Muted)),
            filler(),
            separator() | color(theme::Muted),
            hbox({
                text(" 单次加购数:") | color(theme::Muted),
                filler(),
                qty_dec_btn_->Render(),
                text("  " + std::to_string(add_qty_) + "  ") | bold | color(Color::White),
                qty_inc_btn_->Render()
            }),
            add_cart_btn_->Render(),
            hbox({
                go_cart_btn_->Render(),
                filler(),
                go_coupon_btn_->Render()
            })
        }) | flex;
    } else {
        detail_content = text("未选择商品") | color(theme::Muted) | center | flex;
    }

    Element right_box = panel("选中商品详情与加购", std::move(detail_content), right_focused, PanelStyle::Action)
                        | size(WIDTH, EQUAL, 44);

    Element body = vbox({
        std::move(filter_box),
        hbox({
            std::move(left_box) | flex,
            std::move(right_box)
        }) | flex
    });

    return renderCustomerFrame(
        nav_bar_,
        std::move(body),
        "↑/↓ 移动光标 | Enter 选中 | Tab 进入详情区 | +/- 步进数量 | 1-4 压栈切页 | T 切换周末 | Esc 返回");
}

} // namespace ui
