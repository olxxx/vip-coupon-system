#include "ui/pages/CheckoutPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/MallPage.hpp"
#include "ui/pages/UserCenterPage.hpp"

#include <algorithm>

namespace ui {

using namespace ftxui;

namespace {

std::string timeConstraintText(core::TimeConstraint tc) {
    if (tc == core::TimeConstraint::WEEKDAY_ONLY) return "仅限工作日";
    if (tc == core::TimeConstraint::WEEKEND_ONLY) return "仅限周末";
    return "不限时间";
}

} // namespace

CheckoutPage::CheckoutPage(AppContext& context)
    : Page(context, "已根据平行让利模型自动匹配最优单券方案。按 ↑/↓ 移动光标，Enter 选中方案，再点击「确认支付」。") {
    nav_bar_ = buildCustomerNavBar(0);

    scheme_list_ = DataList(
        &selected_idx_,
        [this] { return 2 + static_cast<int>(eval_items_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            if (idx == 0) {
                std::shared_ptr<shop::Coupon> best = nullptr;
                double saving = 0.0;
                for (const auto& item : eval_items_) {
                    if (item.is_recommended && item.check.is_eligible) {
                        best = item.coupon;
                        saving = item.check.potential_discount;
                        break;
                    }
                }
                if (best) {
                    return hbox({
                        text(" "),
                        col("[★最优推荐]", 13) | bold | color(theme::Good),
                        col(best->getName(), 22) | bold,
                        col(best->getRuleDescription(), 22) | color(Color::GrayLight),
                        text("-" + money(saving)) | bold | color(theme::Good)
                    });
                }
                return hbox({
                    text(" "),
                    col("[★最优推荐]", 13) | color(theme::Accent),
                    text("当前无满足条件的可用优惠券") | color(theme::Muted)
                });
            }
            if (idx == 1) {
                return hbox({
                    text(" "),
                    col("[不使用券]", 13) | color(Color::GrayLight),
                    text("仅享受当前会员等级基础折扣") | color(theme::Muted)
                });
            }

            const auto& item = eval_items_[static_cast<std::size_t>(idx - 2)];
            const auto& c = item.coupon;
            const auto& chk = item.check;

            Element tag_el;
            Element right_el;
            if (chk.is_eligible) {
                tag_el = col(item.is_recommended ? "[★推荐]" : "[✓可用]", 13)
                         | bold | color(theme::Good);
                right_el = text("-" + money(chk.potential_discount)) | bold | color(theme::Good);
            } else {
                tag_el = col("[✗不可用]", 13) | color(theme::Bad);
                std::string reason = chk.fail_reason;
                if (chk.shortfall_amount > 1e-4) {
                    reason = "差" + money(chk.shortfall_amount) + "达标";
                }
                right_el = text("[" + reason + "]") | color(theme::Bad);
            }

            return hbox({
                text(" "),
                std::move(tag_el),
                col(c->getName(), 22) | (chk.is_eligible ? nothing : color(theme::Muted)),
                col(c->getRuleDescription(), 22) | color(theme::Muted),
                std::move(right_el)
            });
        },
        {});

    pay_btn_ = StyledBigButton(
        [this] {
            if (paid_success_) return std::string("支付已完成，返回商城首页");
            std::shared_ptr<shop::Coupon> pick = nullptr;
            const bool auto_opt = (selected_idx_ == 0);
            if (selected_idx_ >= 2 && selected_idx_ - 2 < static_cast<int>(eval_items_.size())) {
                pick = eval_items_[static_cast<std::size_t>(selected_idx_ - 2)].coupon;
            }
            const auto plan = ctx.computeQuote(pick, auto_opt);
            return "确认支付 " + money(plan.final_pay_amount);
        },
        [this] { executePayment(); },
        [] { return ButtonKind::Primary; });

    go_recharge_btn_ = ActionButton("去充值", [this] {
        pending_action = PageAction::push(std::make_unique<UserCenterPage>(ctx));
    });
    go_coupon_btn_ = ActionButton("去领券", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    });
    back_btn_ = ActionButton("返回上页(Esc)", [this] {
        pending_action = PageAction::pop();
    });

    auto sub_btns = Container::Horizontal({go_recharge_btn_, go_coupon_btn_, back_btn_});
    auto right_controls = Container::Vertical({pay_btn_, sub_btns});

    container = Container::Vertical({
        nav_bar_,
        Container::Horizontal({scheme_list_, right_controls})
    });

    syncEvaluations();
    scheme_list_->TakeFocus();
}

void CheckoutPage::on_enter() {
    syncEvaluations();
    scheme_list_->TakeFocus();
}

void CheckoutPage::syncEvaluations() {
    eval_items_.clear();
    if (auto cust = ctx.user_mgr.getCurrentCustomer()) {
        eval_items_ = shop::CouponPlanner::evaluateAndSortAllCoupons(
            ctx.cart, cust->getAvailableCoupons(), cust->getTier(), ctx.current_day);
    }
    const int total_rows = 2 + static_cast<int>(eval_items_.size());
    selected_idx_ = std::clamp(selected_idx_, 0, total_rows - 1);
}

void CheckoutPage::executePayment() {
    if (paid_success_) {
        pending_action = PageAction::reset(std::make_unique<MallPage>(ctx));
        return;
    }
    syncEvaluations();
    std::shared_ptr<shop::Coupon> pick = nullptr;
    const bool use_optimal = (selected_idx_ == 0);
    if (selected_idx_ >= 2 && selected_idx_ - 2 < static_cast<int>(eval_items_.size())) {
        pick = eval_items_[static_cast<std::size_t>(selected_idx_ - 2)].coupon;
    }

    PaymentReceipt rec;
    auto res = ctx.pay(pick, use_optimal, &rec);
    notify(res);
    if (res.success) {
        paid_success_ = true;
        receipt_ = std::move(rec);
        pay_btn_->TakeFocus();
    } else {
        syncEvaluations();
    }
}

bool CheckoutPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (handleCustomerShortcuts(event, 0)) {
            syncEvaluations();
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

Element CheckoutPage::render() {
    syncEvaluations();
    const auto cust = ctx.user_mgr.getCurrentCustomer();

    // 确定当前高亮用于诊断展示的券
    std::shared_ptr<shop::Coupon> inspect_cpn = nullptr;
    if (selected_idx_ == 0) {
        for (const auto& it : eval_items_) {
            if (it.is_recommended && it.check.is_eligible) {
                inspect_cpn = it.coupon;
                break;
            }
        }
    } else if (selected_idx_ >= 2 && selected_idx_ - 2 < static_cast<int>(eval_items_.size())) {
        inspect_cpn = eval_items_[static_cast<std::size_t>(selected_idx_ - 2)].coupon;
    }

    // 左侧底部：四维约束实时诊断面板
    Element diag_box;
    if (inspect_cpn && cust) {
        const double cat_base = ctx.cart.getCategoryAmount(inspect_cpn->getTargetCategory());
        const bool min_ok = core::isAmountGTE(cat_base, inspect_cpn->getMinSpend());
        const bool tier_ok = (cust->getTier() >= inspect_cpn->getMinTier());
        const auto tc = inspect_cpn->getTimeConstraint();
        const bool time_ok = (tc == core::TimeConstraint::ANYTIME) ||
                             (tc == core::TimeConstraint::WEEKDAY_ONLY && !ctx.isWeekend()) ||
                             (tc == core::TimeConstraint::WEEKEND_ONLY && ctx.isWeekend());

        const std::string min_txt = min_ok
            ? ("满" + money(inspect_cpn->getMinSpend()) + " ✓")
            : ("差" + money(inspect_cpn->getMinSpend() - cat_base) + " ✗");
        const std::string tier_txt = core::getTierConfig(inspect_cpn->getMinTier()).name +
                                     (tier_ok ? " ✓" : " ✗");
        const std::string time_txt = timeConstraintText(tc) + (time_ok ? " ✓" : " ✗(按T切换)");

        diag_box = vbox({
            separator() | color(theme::Muted),
            hbox({
                text(" 四维诊断 [" + inspect_cpn->getName() + "]: ") | bold | color(theme::Accent),
                text(inspect_cpn->getTypeDescription()) | color(theme::Muted)
            }),
            hbox({
                text(" [品类] ") | color(theme::Muted),
                text(core::getCategoryName(inspect_cpn->getTargetCategory()) + "(" + money(cat_base) + ")")
                    | color(Color::White),
                text("  [门槛] ") | color(theme::Muted),
                text(min_txt) | bold | color(min_ok ? theme::Good : theme::Bad),
                text("  [等级] ") | color(theme::Muted),
                text(tier_txt) | bold | color(tier_ok ? theme::Good : theme::Bad),
                text("  [时间] ") | color(theme::Muted),
                text(time_txt) | bold | color(time_ok ? theme::Good : theme::Bad)
            })
        });
    } else {
        diag_box = vbox({
            separator() | color(theme::Muted),
            text(selected_idx_ == 1
                     ? " 当前选择：不使用优惠券，仅享受会员等级基础折扣。"
                     : " 提示：背包中若无可用优惠券，可点击右下方 [去领券] 申领后再按 Esc 返回。")
                | color(theme::Muted)
        });
    }

    Element list_header = hbox({
        text("  "),
        col("状态", 13),
        col("券名称", 22),
        col("规则说明", 22),
        text("诊断结果/抵扣额")
    }) | color(theme::Muted);

    Element left_panel = panel(
        "优惠券方案选择与四维诊断",
        vbox({
            std::move(list_header),
            separator() | color(theme::Muted),
            scheme_list_->Render() | flex,
            std::move(diag_box)
        }),
        scheme_list_->Focused(),
        PanelStyle::Primary);

    // 右侧账单面板或支付成功回执卡
    const bool right_focused = pay_btn_->Focused() || go_recharge_btn_->Focused() ||
                               go_coupon_btn_->Focused() || back_btn_->Focused();

    Element right_content;
    if (paid_success_ && receipt_.valid) {
        const double total_saved = receipt_.member_saving + receipt_.coupon_saving;
        Element upgrade_banner = receipt_.upgraded_tier_name.empty()
            ? text(" 会员等级保持不变") | color(theme::Muted)
            : (text(" ★ 恭喜晋升: " + receipt_.old_tier_name + " -> " + receipt_.upgraded_tier_name)
               | bold | color(theme::Warn));

        right_content = vbox({
            text(" ✓ 订单支付已完成 (支付回执单)") | bold | color(theme::Good),
            separator() | color(theme::Muted),
            kv(" 商品原价总额:", money(receipt_.origin_total)),
            kv(" 会员折扣让利:", text("- " + money(receipt_.member_saving)) | color(theme::Good)),
            kv(" 优惠券抵扣 (" + receipt_.coupon_name + "):",
               text("- " + money(receipt_.coupon_saving)) | color(theme::Good)),
            kv(" 本单累计节省:", text(money(total_saved)) | bold | color(theme::Good)),
            separator() | color(theme::Muted),
            kv(" 最终实付金额:", text(money(receipt_.final_paid)) | bold | color(theme::Warn)),
            kv(" 账户余额变动:",
               text(money(receipt_.balance_before) + " -> " + money(receipt_.balance_after))
                   | color(theme::Accent)),
            std::move(upgrade_banner),
            filler(),
            separator() | color(theme::Muted),
            pay_btn_->Render(),
            hbox({
                go_recharge_btn_->Render(),
                filler(),
                go_coupon_btn_->Render(),
                filler(),
                back_btn_->Render()
            })
        }) | flex;
    } else {
        std::shared_ptr<shop::Coupon> pick = nullptr;
        const bool use_optimal = (selected_idx_ == 0);
        if (selected_idx_ >= 2 && selected_idx_ - 2 < static_cast<int>(eval_items_.size())) {
            pick = eval_items_[static_cast<std::size_t>(selected_idx_ - 2)].coupon;
        }
        const auto plan = ctx.computeQuote(pick, use_optimal);
        const double bal = cust ? cust->getBalance() : 0.0;
        const bool bal_ok = core::isAmountGTE(bal, plan.final_pay_amount);
        const std::string tier_name = cust ? cust->getTierName() : "普通会员";
        const std::string used_cpn_name = plan.best_coupon ? plan.best_coupon->getName() : "未使用";

        Element base_row = emptyElement();
        if (inspect_cpn) {
            const double cat_base = ctx.cart.getCategoryAmount(inspect_cpn->getTargetCategory());
            const double diff = std::max(0.0, inspect_cpn->getMinSpend() - cat_base);
            std::string base_str = money(cat_base);
            if (diff > 1e-4) base_str += " (差 " + money(diff) + " 达标)";
            base_row = kv(" 券适用基数 (" + core::getCategoryName(inspect_cpn->getTargetCategory()) + "):",
                          text(base_str) | color(diff > 1e-4 ? theme::Warn : Color::White));
        }

        // 支付后升级预告
        Element upgrade_preview = emptyElement();
        if (cust && !cust->isMaxTier()) {
            const double needed = cust->getAmountNeededForNextTier();
            if (plan.final_pay_amount > 1e-4 && core::isAmountGTE(plan.final_pay_amount, needed)) {
                const auto next_tier = static_cast<core::UserTier>(static_cast<int>(cust->getTier()) + 1);
                upgrade_preview = text(" ↑ 本单支付后将晋升为【" + core::getTierConfig(next_tier).name + "】")
                                  | bold | color(theme::Warn);
            } else {
                upgrade_preview = text(" 本单后距下一级还差 " + money(std::max(0.0, needed - plan.final_pay_amount)))
                                  | color(theme::Muted);
            }
        }

        right_content = vbox({
            kv(" 订单商品原价总额:", money(plan.origin_total)),
            kv(" 会员等级让利 (" + tier_name + "):",
               text("- " + money(plan.member_discount_saving)) | color(theme::Good)),
            kv(" 优惠券抵扣 (" + used_cpn_name + "):",
               text("- " + money(plan.coupon_discount_saving)) | color(theme::Good)),
            std::move(base_row),
            separator() | color(theme::Muted),
            kv(" 最终应付实付总额:",
               text(money(plan.final_pay_amount)) | bold | color(theme::Warn)),
            kv(" 当前账户可用余额:",
               text(money(bal) + (bal_ok ? " (充足)" : " (余额不足)"))
                   | bold | color(bal_ok ? theme::Good : theme::Bad)),
            std::move(upgrade_preview),
            filler(),
            separator() | color(theme::Muted),
            pay_btn_->Render(),
            hbox({
                go_recharge_btn_->Render(),
                filler(),
                go_coupon_btn_->Render(),
                filler(),
                back_btn_->Render()
            })
        }) | flex;
    }

    Element right_panel = panel("收银台账单明细", std::move(right_content), right_focused, PanelStyle::Highlight)
                          | size(WIDTH, EQUAL, 48);

    return renderCustomerFrame(
        nav_bar_,
        hbox({std::move(left_panel) | flex, std::move(right_panel)}),
        "↑/↓ 移动光标 | Enter 选中方案 | Tab 到支付按钮 | T 切换工作日/周末 | Esc 返回上一页");
}

} // namespace ui
