#include "ui/pages/UserCenterPage.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/MallPage.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ui {

using namespace ftxui;

namespace {

std::string tierCodeStr(core::UserTier tier) {
    return "V" + std::to_string(static_cast<int>(tier));
}

std::string rateText(double rate) {
    if (rate >= 0.999) return "无折扣";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (rate * 10.0) << "折";
    return oss.str();
}

} // namespace

UserCenterPage::UserCenterPage(AppContext& context)
    : Page(context, "展示会员成长进度、账户充值、个人卡券包与多态账单流水明细。"),
      cpn_tab_labels_({"全部(0)", "可使用(0)", "已核销(0)"}) {
    nav_bar_ = buildCustomerNavBar(4);

    quick_100_btn_  = ActionButton("+¥100", [this] { notify(ctx.recharge(100.0)); syncData(); });
    quick_500_btn_  = ActionButton("+¥500", [this] { notify(ctx.recharge(500.0)); syncData(); });
    quick_1000_btn_ = ActionButton("+¥1000", [this] { notify(ctx.recharge(1000.0)); syncData(); });

    custom_input_ = TextInput(&recharge_input_, "金额", false, [this] { handleCustomRecharge(); });
    confirm_recharge_btn_ = ActionButton("确认充值", [this] { handleCustomRecharge(); }, true);

    cpn_filter_toggle_ = InlineToggle(&cpn_tab_labels_, &cpn_filter_, [this] {
        cpn_selected_ = 0;
        syncData();
    });

    cpn_list_ = DataList(
        &cpn_selected_,
        [this] { return static_cast<int>(shown_coupons_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& c = shown_coupons_[static_cast<std::size_t>(idx)];
            const bool used = c->isUsed();
            Element left = text(" [" + c->getInstanceId() + "] " + c->getName() +
                                " (" + c->getRuleDescription() + ")");
            if (used) {
                left = std::move(left) | color(theme::Muted) | strikethrough;
            }
            Element right = text(used ? "[已核销] " : "[可使用] ")
                            | bold | color(used ? theme::Muted : theme::Good);
            return hbox({std::move(left), filler(), std::move(right)});
        },
        {},
        "当前筛选下暂无优惠券。");

    clean_used_btn_ = ActionButton("清理已核销券", [this] {
        notify(ctx.cleanUsedCoupons());
        syncData();
    });
    go_coupon_btn_ = ActionButton("去领券中心", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    });

    tx_list_ = DataList(
        &tx_selected_,
        [this] { return static_cast<int>(recent_txs_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto* tx = recent_txs_[static_cast<std::size_t>(idx)];
            if (!tx) return emptyElement();
            Color c = Color::GrayLight;
            switch (tx->getType()) {
                case user::TransactionType::RECHARGE:        c = theme::Good; break;
                case user::TransactionType::TIER_UPGRADE:    c = theme::Warn; break;
                case user::TransactionType::PURCHASE_COUPON: c = theme::Accent; break;
                case user::TransactionType::EXPENSE:         c = Color::White; break;
            }
            return text(" " + tx->getFormattedText()) | color(c);
        },
        {},
        "暂无账单流水记录。");

    go_mall_btn_ = ActionButton("去商城选购", [this] {
        pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
    });
    go_cart_btn_ = ActionButton("去购物车结算", [this] {
        pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
    });

    auto recharge_quick = Container::Horizontal({quick_100_btn_, quick_500_btn_, quick_1000_btn_});
    auto recharge_custom = Container::Horizontal({custom_input_, confirm_recharge_btn_});
    auto recharge_box = Container::Vertical({recharge_quick, recharge_custom});

    auto cpn_box = Container::Vertical({
        cpn_filter_toggle_,
        cpn_list_,
        Container::Horizontal({clean_used_btn_, go_coupon_btn_})
    });

    auto tx_box = Container::Vertical({
        tx_list_,
        Container::Horizontal({go_mall_btn_, go_cart_btn_})
    });

    container = Container::Vertical({
        nav_bar_,
        recharge_box,
        Container::Horizontal({cpn_box, tx_box})
    });

    syncData();
    quick_500_btn_->TakeFocus();
}

void UserCenterPage::on_enter() {
    syncData();
}

void UserCenterPage::syncData() {
    shown_coupons_.clear();
    recent_txs_.clear();

    const auto cust = ctx.user_mgr.getCurrentCustomer();
    if (!cust) return;

    const auto& all_cpns = cust->getCoupons();
    const auto avail_cpns = cust->getAvailableCoupons();
    const auto used_cpns = cust->getUsedCoupons();

    cpn_tab_labels_ = {
        "全部(" + std::to_string(all_cpns.size()) + ")",
        "可使用(" + std::to_string(avail_cpns.size()) + ")",
        "已核销(" + std::to_string(used_cpns.size()) + ")"
    };

    if (cpn_filter_ == 1) {
        shown_coupons_ = avail_cpns;
    } else if (cpn_filter_ == 2) {
        shown_coupons_ = used_cpns;
    } else {
        shown_coupons_ = all_cpns;
    }

    recent_txs_ = cust->getRecentTransactions(100);
}

void UserCenterPage::handleCustomRecharge() {
    double amt = 0.0;
    if (!parseAmount(recharge_input_, amt, false)) {
        setStatus("充值金额无效：请输入大于 0 的数字（最多两位小数）", true);
        return;
    }
    notifyImportant(ctx.recharge(amt, "个人中心自定义充值"));
    syncData();
}

bool UserCenterPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (handleCustomerShortcuts(event, 4)) {
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

Element UserCenterPage::render() {
    syncData();
    const auto cust = ctx.user_mgr.getCurrentCustomer();
    if (!cust) {
        return text("请先登录消费者账号") | center;
    }

    const auto tier = cust->getTier();
    const auto tcfg = core::getTierConfig(tier);
    const double progress = cust->getGrowthProgress();
    const int pct = static_cast<int>(std::round(progress * 100.0));

    std::string next_hint;
    if (cust->isMaxTier()) {
        next_hint = "已达到最高等级【至尊会员 V5】";
    } else {
        const auto next_tier = static_cast<core::UserTier>(static_cast<int>(tier) + 1);
        const auto ncfg = core::getTierConfig(next_tier);
        next_hint = "距离下一级【" + ncfg.name + " (满" + money(cust->getNextTierThreshold()) +
                    ")】还差: " + money(cust->getAmountNeededForNextTier());
    }

    Element growth_panel = panel(
        "会员等级与成长值进度 (注册于: " + cust->getCreatedAt() + ")",
        vbox({
            hbox({
                text("当前等级: ") | color(theme::Muted),
                text(tierCodeStr(tier) + " " + tcfg.name + " (" + rateText(tcfg.discount_rate) + ")")
                    | bold | color(theme::Warn),
                filler(),
                text("累计消费: ") | color(theme::Muted),
                text(money(cust->getTotalSpent())) | bold | color(Color::White)
            }),
            text(next_hint) | color(Color::GrayLight),
            hbox({
                text("成长进度: ") | color(theme::Accent),
                gauge(static_cast<float>(progress)) | color(theme::Accent) | flex,
                text(" " + std::to_string(pct) + "%") | bold | color(theme::Accent)
            })
        }),
        false);

    const bool recharge_focused = quick_100_btn_->Focused() || quick_500_btn_->Focused() ||
                                  quick_1000_btn_->Focused() || custom_input_->Focused() ||
                                  confirm_recharge_btn_->Focused();

    Element recharge_panel = panel(
        "账户余额充值",
        vbox({
            hbox({
                text("当前可用余额: ") | color(theme::Muted),
                text(money(cust->getBalance())) | bold | color(theme::Good),
                filler(),
                text("快捷: ") | color(theme::Muted),
                quick_100_btn_->Render(),
                quick_500_btn_->Render(),
                quick_1000_btn_->Render()
            }),
            separator() | color(theme::Muted),
            hbox({
                text("自定义金额: ") | color(theme::Muted),
                custom_input_->Render() | size(WIDTH, EQUAL, 16),
                filler(),
                confirm_recharge_btn_->Render()
            })
        }),
        recharge_focused);

    const bool cpn_focused = cpn_filter_toggle_->Focused() || cpn_list_->Focused() ||
                             clean_used_btn_->Focused() || go_coupon_btn_->Focused();

    Element cpn_panel = panel(
        "我的卡券背包",
        vbox({
            cpn_filter_toggle_->Render(),
            separator() | color(theme::Muted),
            cpn_list_->Render() | flex,
            separator() | color(theme::Muted),
            hbox({clean_used_btn_->Render() | flex, go_coupon_btn_->Render() | flex}) | hcenter
        }),
        cpn_focused);

    const bool tx_focused = tx_list_->Focused() || go_mall_btn_->Focused() || go_cart_btn_->Focused();

    Element tx_panel = panel(
        "账单流水历史 (" + std::to_string(cust->getTransactionCount()) + "条)",
        vbox({
            tx_list_->Render() | flex,
            separator() | color(theme::Muted),
            hbox({filler(), go_mall_btn_->Render(), go_cart_btn_->Render()})
        }),
        tx_focused);

    Element body = vbox({
        hbox({
            std::move(growth_panel) | flex,
            std::move(recharge_panel) | size(WIDTH, EQUAL, 54)
        }),
        hbox({
            std::move(cpn_panel) | size(WIDTH, EQUAL, 54),
            std::move(tx_panel) | flex
        }) | flex
    });

    return renderCustomerFrame(
        nav_bar_,
        std::move(body),
        "Tab/方向键 切换区域 | 支持快捷充值与清理已核销券 | Esc 返回上一页");
}

} // namespace ui
