#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/UserCenterPage.hpp"

#include <algorithm>

namespace ui {

using namespace ftxui;

namespace {

std::string timeLimitName(core::TimeConstraint tc) {
    if (tc == core::TimeConstraint::WEEKDAY_ONLY) return "仅限工作日";
    if (tc == core::TimeConstraint::WEEKEND_ONLY) return "仅限周末";
    return "不限时间";
}

} // namespace

CouponSquarePage::CouponSquarePage(AppContext& context)
    : Page(context, "支持按 [全部/免费/付费/我可领] 筛选，每种优惠券(按模板ID)限持有一张可用券。"),
      filter_labels_({"全部", "免费申领", "付费购买", "我可领"}) {
    nav_bar_ = buildCustomerNavBar(3);

    filter_toggle_ = InlineToggle(&filter_labels_, &filter_mode_, [this] {
        selected_idx_ = 0;
        syncPool();
    });

    claim_all_btn_ = ActionButton("一键领取全部免费券 (A)", [this] { handleClaimAllFree(); }, true);

    pool_list_ = DataList(
        &selected_idx_,
        [this] { return static_cast<int>(filtered_pool_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& c = filtered_pool_[static_cast<std::size_t>(idx)];
            const auto cust = ctx.user_mgr.getCurrentCustomer();
            const bool owned = cust ? ctx.cpn_mgr.hasUserHeldCoupon(*cust, c->getTemplateId()) : false;

            Element cost_el = c->isFree()
                ? (col("[免费申领]", 12) | color(theme::Good))
                : (col("[" + money(c->getCost()) + "购]", 12) | color(theme::Warn));

            Element own_el = owned
                ? (col("[已拥有]", 10) | bold | color(theme::Warn))
                : col("", 10);

            return hbox({
                text(" "),
                std::move(cost_el),
                std::move(own_el),
                col(c->getName(), 22) | (owned ? color(theme::Muted) : bold),
                text(c->getRuleDescription()) | color(Color::GrayLight)
            });
        },
        {},
        "该筛选条件下暂无优惠券模板");

    acquire_btn_ = StyledButton(
        [this] {
            std::string lbl = "免费申领该优惠券";
            if (!filtered_pool_.empty() && selected_idx_ >= 0 &&
                selected_idx_ < static_cast<int>(filtered_pool_.size())) {
                const auto& c = filtered_pool_[static_cast<std::size_t>(selected_idx_)];
                const auto cust = ctx.user_mgr.getCurrentCustomer();
                const bool owned = cust && ctx.cpn_mgr.hasUserHeldCoupon(*cust, c->getTemplateId());
                if (owned) {
                    lbl = "已拥有该券 (不可重复获取)";
                } else if (!c->isFree()) {
                    lbl = "支付 " + money(c->getCost()) + " 购买该券";
                }
            }
            return lbl;
        },
        [this] { handleAcquireSelected(); },
        [this] {
            if (!filtered_pool_.empty() && selected_idx_ >= 0 &&
                selected_idx_ < static_cast<int>(filtered_pool_.size())) {
                const auto& c = filtered_pool_[static_cast<std::size_t>(selected_idx_)];
                const auto cust = ctx.user_mgr.getCurrentCustomer();
                if (cust && ctx.cpn_mgr.hasUserHeldCoupon(*cust, c->getTemplateId())) return ButtonKind::Disabled;
            }
            return ButtonKind::Primary;
        });

    go_wallet_btn_ = ActionButton("查看我的券包", [this] {
        pending_action = PageAction::push(std::make_unique<UserCenterPage>(ctx));
    });
    go_cart_btn_ = ActionButton("去购物车结算", [this] {
        pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
    });

    auto top_filter_bar = Container::Horizontal({filter_toggle_, claim_all_btn_});
    auto right_controls = Container::Vertical({
        acquire_btn_,
        Container::Horizontal({go_wallet_btn_, go_cart_btn_})
    });

    container = Container::Vertical({
        nav_bar_,
        top_filter_bar,
        Container::Horizontal({pool_list_, right_controls})
    });

    syncPool();
    pool_list_->TakeFocus();
}

void CouponSquarePage::on_enter() {
    syncPool();
    pool_list_->TakeFocus();
}

void CouponSquarePage::syncPool() {
    const auto cust = ctx.user_mgr.getCurrentCustomer();
    const auto& all = ctx.cpn_mgr.getPublicPool();
    const auto free_list = ctx.cpn_mgr.getFreeCoupons();
    const auto paid_list = ctx.cpn_mgr.getPaidCoupons();
    const auto elig_list = cust ? ctx.cpn_mgr.getEligibleCouponsForUser(*cust)
                                : std::vector<std::shared_ptr<shop::Coupon>>{};

    filter_labels_ = {
        "全部(" + std::to_string(all.size()) + ")",
        "免费申领(" + std::to_string(free_list.size()) + ")",
        "付费购买(" + std::to_string(paid_list.size()) + ")",
        "我可领(" + std::to_string(elig_list.size()) + ")"
    };

    if (filter_mode_ == 1) {
        filtered_pool_ = free_list;
    } else if (filter_mode_ == 2) {
        filtered_pool_ = paid_list;
    } else if (filter_mode_ == 3) {
        filtered_pool_ = elig_list;
    } else {
        filtered_pool_ = all;
    }

    if (filtered_pool_.empty()) {
        selected_idx_ = 0;
    } else {
        selected_idx_ = std::clamp(selected_idx_, 0, static_cast<int>(filtered_pool_.size()) - 1);
    }
}

void CouponSquarePage::handleAcquireSelected() {
    syncPool();
    if (filtered_pool_.empty()) {
        setStatus("当前无可选优惠券", true);
        return;
    }
    const auto& sel = filtered_pool_[static_cast<std::size_t>(selected_idx_)];
    notify(ctx.claimOrBuyCoupon(sel->getTemplateId()));
    syncPool();
}

void CouponSquarePage::handleClaimAllFree() {
    notifyImportant(ctx.claimAllFreeCoupons());
    syncPool();
}

bool CouponSquarePage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (event == Event::Character('a') || event == Event::Character('A')) {
            handleClaimAllFree();
            handled = true;
        } else if (handleCustomerShortcuts(event, 3)) {
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

Element CouponSquarePage::render() {
    syncPool();
    const auto cust = ctx.user_mgr.getCurrentCustomer();

    Element filter_box = panel("券池筛选与快捷操作", hbox({
        filter_toggle_->Render(),
        filler(),
        claim_all_btn_->Render()
    }), filter_toggle_->Focused() || claim_all_btn_->Focused());

    Element table_header = hbox({
        text("  "),
        col("获取方式", 12),
        col("持有状态", 10),
        col("券面名称", 22),
        text("抵扣规则")
    }) | color(theme::Muted);

    Element left_box = panel(
        "公共优惠券池列表",
        vbox({
            std::move(table_header),
            separator() | color(theme::Muted),
            pool_list_->Render() | flex
        }),
        pool_list_->Focused());

    const bool right_focused = acquire_btn_->Focused() || go_wallet_btn_->Focused() || go_cart_btn_->Focused();
    Element detail_content;

    if (!filtered_pool_.empty() && selected_idx_ >= 0 &&
        selected_idx_ < static_cast<int>(filtered_pool_.size())) {
        const auto& sel = filtered_pool_[static_cast<std::size_t>(selected_idx_)];
        const bool tier_ok = cust && (cust->getTier() >= sel->getMinTier());
        const bool owned = cust && ctx.cpn_mgr.hasUserHeldCoupon(*cust, sel->getTemplateId());
        const std::string min_tier_name = core::getTierConfig(sel->getMinTier()).name;

        detail_content = vbox({
            kv("模板编号:", sel->getTemplateId()),
            kv("券面名称:", text(sel->getName()) | bold | color(Color::White)),
            kv("券种类型:", sel->getTypeDescription()),
            kv("抵扣规则:", text(sel->getRuleDescription()) | bold | color(theme::Warn)),
            separator() | color(theme::Muted),
            kv("适用品类:", core::getCategoryName(sel->getTargetCategory())),
            kv("最低等级:", text(min_tier_name + (tier_ok ? " (已达标)" : " (未达标)"))
                            | bold | color(tier_ok ? theme::Good : theme::Bad)),
            kv("时间限制:", timeLimitName(sel->getTimeConstraint())),
            kv("领券费用:", sel->isFree() ? (text("免费") | color(theme::Good))
                                          : (text(money(sel->getCost())) | color(theme::Warn))),
            kv("持有状态:", text(owned ? "已拥有 (限持一张)" : "未持有 (可获取)")
                            | bold | color(owned ? theme::Warn : theme::Good)),
            filler(),
            separator() | color(theme::Muted),
            acquire_btn_->Render() | hcenter,
            hbox({go_wallet_btn_->Render(), go_cart_btn_->Render()}) | hcenter
        }) | flex;
    } else {
        detail_content = text("未选择优惠券") | color(theme::Muted);
    }

    Element right_box = panel("优惠券约束详情与操作", std::move(detail_content), right_focused)
                        | size(WIDTH, EQUAL, 46);

    Element body = vbox({
        std::move(filter_box),
        hbox({std::move(left_box) | flex, std::move(right_box)}) | flex
    });

    return renderCustomerFrame(
        nav_bar_,
        std::move(body),
        "↑/↓ 移动光标 | Enter 选中 | Tab 到领取按钮 | A 一键领免费券 | Esc 返回上一页");
}

} // namespace ui
