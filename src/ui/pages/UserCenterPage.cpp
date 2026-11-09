#include "ui/pages/UserCenterPage.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/MallPage.hpp"

#include <ftxui/dom/linear_gradient.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ui {

using namespace ftxui;

namespace {

std::string tierCodeStr(core::UserTier tier) {
    return "V" + std::to_string(static_cast<int>(tier));
}

std::string rateText(double rate) {
    if (rate >= 0.999) return "无折扣(10折)";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (rate * 10.0) << "折";
    return oss.str();
}

std::string timeLimitText(core::TimeConstraint tc) {
    if (tc == core::TimeConstraint::WEEKDAY_ONLY) return "仅限工作日";
    if (tc == core::TimeConstraint::WEEKEND_ONLY) return "仅限周末";
    return "不限时间";
}

} // namespace

UserCenterPage::UserCenterPage(AppContext& context)
    : Page(context, "个人中心：可通过顶部子页签或 [ / ] 切换「账户充值与安全」、「我的卡券背包」、「账单流水历史」。"),
      cpn_tab_labels_({"全部(0)", "可使用(0)", "已核销(0)"}),
      tx_filter_labels_({"全部(0)", "充值(0)", "消费(0)", "购券(0)", "升级(0)"}) {
    nav_bar_ = buildCustomerNavBar(4);

    // 顶部子页签按钮
    auto make_sub_tab = [this](std::function<std::string()> label_fn, int target) {
        return StyledButton(
            std::move(label_fn),
            [this, target] {
                sub_tab_ = target;
                syncData();
            },
            [this, target] { return sub_tab_ == target ? ButtonKind::TabActive : ButtonKind::Tab; });
    };

    auto sub0_btn = make_sub_tab([] { return "账户充值与安全"; }, 0);
    auto sub1_btn = make_sub_tab([this] {
        const auto cust = ctx.user_mgr.getCurrentCustomer();
        const std::size_t n = cust ? cust->getCouponCount() : 0;
        return "我的卡券背包(" + std::to_string(n) + ")";
    }, 1);
    auto sub2_btn = make_sub_tab([this] {
        const auto cust = ctx.user_mgr.getCurrentCustomer();
        const std::size_t n = cust ? cust->getTransactionCount() : 0;
        return "账单流水历史(" + std::to_string(n) + ")";
    }, 2);

    auto sub_tab_row = Container::Horizontal({sub0_btn, sub1_btn, sub2_btn});
    sub_tab_bar_ = Renderer(sub_tab_row, [=] {
        return hbox({
            filler(),
            sub0_btn->Render(),
            text("   "),
            sub1_btn->Render(),
            text("   "),
            sub2_btn->Render(),
            filler()
        });
    });

    // --- Tab 0: 账户充值与修改密码 ---
    quick_100_btn_  = ActionButton("+¥100", [this] { notify(ctx.recharge(100.0)); syncData(); });
    quick_500_btn_  = ActionButton("+¥500", [this] { notify(ctx.recharge(500.0)); syncData(); });
    quick_1000_btn_ = ActionButton("+¥1000", [this] { notify(ctx.recharge(1000.0)); syncData(); });

    custom_input_ = TextInput(&recharge_input_, "充值金额", false, [this] { handleCustomRecharge(); });
    confirm_recharge_btn_ = ActionButton("确认充值", [this] { handleCustomRecharge(); }, true);

    old_pwd_comp_ = TextInput(&old_pwd_input_, "输入原密码", true);
    new_pwd_comp_ = TextInput(&new_pwd_input_, "输入新密码", true, [this] { handleChangePassword(); });
    change_pwd_btn_ = BigActionButton("确认修改密码", [this] { handleChangePassword(); }, ButtonKind::Primary);

    auto tab0_container = Container::Vertical({
        Container::Horizontal({quick_100_btn_, quick_500_btn_, quick_1000_btn_}),
        Container::Horizontal({custom_input_, confirm_recharge_btn_}),
        Container::Horizontal({old_pwd_comp_, new_pwd_comp_, change_pwd_btn_})
    });

    // --- Tab 1: 我的卡券背包 ---
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
            Element status_el = col(used ? "[已核销]" : "[可使用]", 11)
                                | bold | color(used ? theme::Muted : theme::Good);
            Element main_el = hbox({
                col(c->getName(), 22) | (used ? color(theme::Muted) : bold),
                text(c->getRuleDescription()) | color(used ? theme::Muted : Color::GrayLight)
            });
            if (used) {
                main_el = std::move(main_el) | strikethrough;
            }
            return hbox({text(" "), std::move(status_el), std::move(main_el)});
        },
        [this] {
            syncData();
            if (!shown_coupons_.empty()) {
                const auto& c = shown_coupons_[static_cast<std::size_t>(cpn_selected_)];
                setStatus("已选中卡券「" + c->getName() +
                          "」，按 Del/D 可直接移除，或按 → 进入右侧操作区。", false);
            }
        },
        "当前筛选下暂无优惠券。可点击右侧「去领券中心」免费申领。");

    del_cpn_btn_    = DangerButton("删除选中卡券(Del)", [this] { handleRemoveSelectedCoupon(); });
    clean_used_btn_ = ActionButton("清理已核销券(C)", [this] { handleCleanUsedCoupons(); });
    clear_cpn_btn_  = DangerButton("清空全部卡券", [this] { handleClearAllCoupons(); });
    go_coupon_btn_  = BigActionButton("去领券中心", [this] {
        pending_action = PageAction::push(std::make_unique<CouponSquarePage>(ctx));
    }, ButtonKind::Primary);

    auto cpn_right_ops = Container::Vertical({
        Container::Horizontal({del_cpn_btn_, clean_used_btn_}),
        Container::Horizontal({clear_cpn_btn_, go_coupon_btn_})
    });

    auto tab1_container = Container::Vertical({
        cpn_filter_toggle_,
        Container::Horizontal({cpn_list_, cpn_right_ops})
    });

    // --- Tab 2: 账单流水历史 ---
    tx_filter_toggle_ = InlineToggle(&tx_filter_labels_, &tx_filter_, [this] {
        tx_selected_ = 0;
        syncData();
    });

    tx_list_ = DataList(
        &tx_selected_,
        [this] { return static_cast<int>(shown_txs_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto* tx = shown_txs_[static_cast<std::size_t>(idx)];
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
        [this] {
            syncData();
            if (!shown_txs_.empty()) {
                const auto* tx = shown_txs_[static_cast<std::size_t>(tx_selected_)];
                if (tx) {
                    setStatus("已选中流水记录，右侧面板可查看完整交易凭证。", false);
                }
            }
        },
        "当前筛选下暂无账单流水记录。");

    go_mall_btn_ = ActionButton("去商城选购", [this] {
        pending_action = PageAction::push(std::make_unique<MallPage>(ctx));
    });
    go_cart_btn_ = ActionButton("去购物车结算", [this] {
        pending_action = PageAction::push(std::make_unique<CartPage>(ctx));
    }, true);

    auto tab2_container = Container::Vertical({
        tx_filter_toggle_,
        Container::Horizontal({
            tx_list_,
            Container::Horizontal({go_mall_btn_, go_cart_btn_})
        })
    });

    sub_tab_container_ = Container::Tab({tab0_container, tab1_container, tab2_container}, &sub_tab_);

    container = Container::Vertical({
        nav_bar_,
        sub_tab_bar_,
        sub_tab_container_
    });

    syncData();
    quick_500_btn_->TakeFocus();
}

void UserCenterPage::on_enter() {
    syncData();
}

void UserCenterPage::syncData() {
    shown_coupons_.clear();
    shown_txs_.clear();

    const auto cust = ctx.user_mgr.getCurrentCustomer();
    if (!cust) return;

    // 1. 同步卡券列表
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

    if (shown_coupons_.empty()) {
        cpn_selected_ = 0;
    } else {
        cpn_selected_ = std::clamp(cpn_selected_, 0, static_cast<int>(shown_coupons_.size()) - 1);
    }

    // 2. 同步流水列表与分类统计
    const auto all_txs = cust->getRecentTransactions(200);
    std::size_t n_rec = 0, n_exp = 0, n_cpn = 0, n_upg = 0;
    for (const auto* tx : all_txs) {
        if (!tx) continue;
        switch (tx->getType()) {
            case user::TransactionType::RECHARGE:        ++n_rec; break;
            case user::TransactionType::EXPENSE:         ++n_exp; break;
            case user::TransactionType::PURCHASE_COUPON: ++n_cpn; break;
            case user::TransactionType::TIER_UPGRADE:    ++n_upg; break;
        }
        const bool match = (tx_filter_ == 0) ||
                           (tx_filter_ == 1 && tx->getType() == user::TransactionType::RECHARGE) ||
                           (tx_filter_ == 2 && tx->getType() == user::TransactionType::EXPENSE) ||
                           (tx_filter_ == 3 && tx->getType() == user::TransactionType::PURCHASE_COUPON) ||
                           (tx_filter_ == 4 && tx->getType() == user::TransactionType::TIER_UPGRADE);
        if (match) {
            shown_txs_.push_back(tx);
        }
    }

    tx_filter_labels_ = {
        "全部(" + std::to_string(all_txs.size()) + ")",
        "充值(" + std::to_string(n_rec) + ")",
        "消费(" + std::to_string(n_exp) + ")",
        "购券(" + std::to_string(n_cpn) + ")",
        "升级(" + std::to_string(n_upg) + ")"
    };

    if (shown_txs_.empty()) {
        tx_selected_ = 0;
    } else {
        tx_selected_ = std::clamp(tx_selected_, 0, static_cast<int>(shown_txs_.size()) - 1);
    }
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

void UserCenterPage::handleChangePassword() {
    auto res = ctx.changePassword(old_pwd_input_, new_pwd_input_);
    notifyImportant(res);
    if (res.success) {
        old_pwd_input_.clear();
        new_pwd_input_.clear();
    }
}

void UserCenterPage::handleRemoveSelectedCoupon() {
    syncData();
    if (shown_coupons_.empty()) {
        setStatus("当前列表中没有可删除的优惠券", true);
        return;
    }
    const auto inst_id = shown_coupons_[static_cast<std::size_t>(cpn_selected_)]->getInstanceId();
    notify(ctx.removeUserCoupon(inst_id));
    syncData();
}

void UserCenterPage::handleCleanUsedCoupons() {
    notify(ctx.cleanUsedCoupons());
    syncData();
}

void UserCenterPage::handleClearAllCoupons() {
    notifyImportant(ctx.clearAllCoupons());
    syncData();
}

bool UserCenterPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (event == Event::F1) {
            sub_tab_ = 0;
            syncData();
            handled = true;
        } else if (event == Event::F2) {
            sub_tab_ = 1;
            syncData();
            handled = true;
        } else if (event == Event::F3) {
            sub_tab_ = 2;
            syncData();
            handled = true;
        } else if (event == Event::Character('[')) {
            sub_tab_ = (sub_tab_ + 2) % 3;
            syncData();
            handled = true;
        } else if (event == Event::Character(']')) {
            sub_tab_ = (sub_tab_ + 1) % 3;
            syncData();
            handled = true;
        } else if (event == Event::Character('c') || event == Event::Character('C')) {
            handleCleanUsedCoupons();
            handled = true;
        } else if (sub_tab_ == 1 && (event == Event::Delete || event == Event::Backspace ||
                                     event == Event::Character('d') || event == Event::Character('D'))) {
            handleRemoveSelectedCoupon();
            handled = true;
        } else if (handleCustomerShortcuts(event, 4)) {
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

Element UserCenterPage::renderSubTabBar() {
    return panel("个人中心模块切换", sub_tab_bar_->Render(), sub_tab_bar_->Focused(), PanelStyle::SubNav);
}

Element UserCenterPage::renderAccountTab() {
    const auto cust = ctx.user_mgr.getCurrentCustomer();
    const auto tier = cust->getTier();
    const auto tcfg = core::getTierConfig(tier);
    const double progress = cust->getGrowthProgress();
    const int pct = static_cast<int>(std::round(progress * 100.0));

    std::string next_hint;
    if (cust->isMaxTier()) {
        next_hint = "★ 已达到最高等级【至尊会员 V5】，享受全场 8.0 折最高礼遇！";
    } else {
        const auto next_tier = static_cast<core::UserTier>(static_cast<int>(tier) + 1);
        const auto ncfg = core::getTierConfig(next_tier);
        next_hint = "距离下一级【" + ncfg.name + " (满" + money(cust->getNextTierThreshold()) +
                    ")】还差消费: " + money(cust->getAmountNeededForNextTier());
    }

    Elements ladder_rows;
    ladder_rows.push_back(hbox({
        text(" "),
        col("等级", 6),
        col("称号", 12),
        col("累计消费门槛", 16),
        col("专享折扣", 14),
        text("状态")
    }) | color(theme::Muted));
    ladder_rows.push_back(separator() | color(theme::Muted));

    for (int v = 0; v <= 5; ++v) {
        const auto vt = static_cast<core::UserTier>(v);
        const auto cfg = core::getTierConfig(vt);
        const bool is_cur = (vt == tier);
        const bool reached = (static_cast<int>(tier) >= v);
        Element status_tag = is_cur
            ? (text("◀ 当前等级") | bold | color(theme::Warn))
            : (reached ? (text("✓ 已达成") | color(theme::Good))
                       : (text("未解锁") | color(theme::Muted)));
        Element row = hbox({
            text(" "),
            col("V" + std::to_string(v), 6) | color(is_cur ? theme::Warn : Color::GrayLight),
            col(cfg.name, 12) | (is_cur ? (bold | color(theme::Warn)) : nothing),
            col(money(cfg.upgrade_threshold), 16),
            col(rateText(cfg.discount_rate), 14) | color(theme::Good),
            std::move(status_tag)
        });
        ladder_rows.push_back(std::move(row));
    }

    Element grad_gauge = gauge(static_cast<float>(progress))
                         | color(LinearGradient().Angle(0).Stop(theme::Accent).Stop(theme::Good))
                         | flex;

    Element left_panel = panel(
        "会员成长进度与等级权益阶梯",
        vbox({
            hbox({
                text(" 当前等级: ") | color(theme::Muted),
                text(tierCodeStr(tier) + " " + tcfg.name + " (" + rateText(tcfg.discount_rate) + ")")
                    | bold | color(theme::Warn),
                filler(),
                text("累计消费: ") | color(theme::Muted),
                text(money(cust->getTotalSpent())) | bold | color(Color::White)
            }),
            text(" " + next_hint) | color(Color::GrayLight),
            hbox({
                text(" 成长进度: ") | color(theme::Accent),
                dbox({
                    std::move(grad_gauge),
                    text(" " + std::to_string(pct) + "% ") | bold | color(Color::White) | center
                }) | flex
            }),
            separator() | color(theme::Muted),
            text(" 会员等级权益阶梯一览:") | bold | color(Color::GrayLight),
            vbox(std::move(ladder_rows)) | flex
        }),
        false,
        PanelStyle::Highlight);

    const bool recharge_focused = quick_100_btn_->Focused() || quick_500_btn_->Focused() ||
                                  quick_1000_btn_->Focused() || custom_input_->Focused() ||
                                  confirm_recharge_btn_->Focused();

    Element recharge_panel = panel(
        "账户余额充值",
        vbox({
            kv(" 当前可用余额:", text(money(cust->getBalance())) | bold | color(theme::Good)),
            separator() | color(theme::Muted),
            hbox({
                text(" 快捷面额: ") | color(theme::Muted),
                quick_100_btn_->Render(),
                text("  "),
                quick_500_btn_->Render(),
                text("  "),
                quick_1000_btn_->Render()
            }),
            hbox({
                text(" 自定义额: ") | color(theme::Muted),
                custom_input_->Render() | size(WIDTH, EQUAL, 18),
                filler(),
                confirm_recharge_btn_->Render()
            })
        }),
        recharge_focused,
        PanelStyle::Action);

    const bool pwd_focused = old_pwd_comp_->Focused() || new_pwd_comp_->Focused() || change_pwd_btn_->Focused();

    Element pwd_panel = panel(
        "账户安全与登录密码修改",
        vbox({
            kv(" 当前登录账号:", text(cust->getUsername() + " (注册于 " + cust->getCreatedAt() + ")")
                                 | bold | color(Color::White)),
            hbox({
                text(" 原密码: ") | color(theme::Muted),
                old_pwd_comp_->Render() | flex
            }),
            hbox({
                text(" 新密码: ") | color(theme::Muted),
                new_pwd_comp_->Render() | flex
            }),
            filler(),
            change_pwd_btn_->Render()
        }) | flex,
        pwd_focused,
        PanelStyle::Action);

    return hbox({
        std::move(left_panel) | flex,
        vbox({
            std::move(recharge_panel),
            std::move(pwd_panel) | flex
        }) | size(WIDTH, EQUAL, 52)
    }) | flex;
}

Element UserCenterPage::renderCouponBagTab() {
    const auto cust = ctx.user_mgr.getCurrentCustomer();

    Element filter_box = panel("背包卡券筛选与统计", hbox({
        cpn_filter_toggle_->Render(),
        filler(),
        text("提示: 按 Del/D 删除选中券，按 C 清理已核销券 ") | color(theme::Muted)
    }), cpn_filter_toggle_->Focused(), PanelStyle::SubNav);

    Element table_header = hbox({
        text("  "),
        col("状态", 11),
        col("券面名称", 22),
        text("抵扣规则")
    }) | color(theme::Muted);

    Element left_box = panel(
        "我的卡券背包列表",
        vbox({
            std::move(table_header),
            separator() | color(theme::Muted),
            cpn_list_->Render() | flex
        }),
        cpn_list_->Focused(),
        PanelStyle::Primary);

    const bool right_focused = del_cpn_btn_->Focused() || clean_used_btn_->Focused() ||
                               clear_cpn_btn_->Focused() || go_coupon_btn_->Focused();

    Element detail_box;
    if (!shown_coupons_.empty() && cpn_selected_ >= 0 &&
        cpn_selected_ < static_cast<int>(shown_coupons_.size())) {
        const auto& sel = shown_coupons_[static_cast<std::size_t>(cpn_selected_)];
        const bool tier_ok = cust && (cust->getTier() >= sel->getMinTier());
        detail_box = vbox({
            kv(" 券面名称:", text(sel->getName()) | bold | color(Color::White)),
            kv(" 券种类型:", sel->getTypeDescription()),
            kv(" 抵扣规则:", text(sel->getRuleDescription()) | bold | color(theme::Warn)),
            separator() | color(theme::Muted),
            kv(" 适用品类:", core::getCategoryName(sel->getTargetCategory())),
            kv(" 最低等级:", text(core::getTierConfig(sel->getMinTier()).name +
                                 (tier_ok ? " (已达标)" : " (未达标)"))
                            | color(tier_ok ? theme::Good : theme::Bad)),
            kv(" 时间限制:", timeLimitText(sel->getTimeConstraint())),
            kv(" 当前状态:", text(sel->isUsed() ? "已核销 (不可再用)" : "可使用")
                            | bold | color(sel->isUsed() ? theme::Muted : theme::Good))
        });
    } else {
        detail_box = vbox({
            text(" 当前未选中任何优惠券") | color(theme::Muted),
            text(" 可点击下方「去领券中心」获取优惠券。") | color(theme::Muted)
        });
    }

    Element right_box = panel(
        "选中卡券详情与背包操作",
        vbox({
            std::move(detail_box),
            filler(),
            separator() | color(theme::Muted),
            text(" 卡券清理与删除操作:") | bold | color(Color::GrayLight),
            hbox({del_cpn_btn_->Render(), filler(), clean_used_btn_->Render()}),
            hbox({clear_cpn_btn_->Render(), filler()}),
            go_coupon_btn_->Render()
        }) | flex,
        right_focused,
        PanelStyle::Action) | size(WIDTH, EQUAL, 48);

    return vbox({
        std::move(filter_box),
        hbox({std::move(left_box) | flex, std::move(right_box)}) | flex
    }) | flex;
}

Element UserCenterPage::renderTransactionTab() {
    Element filter_box = panel("账单流水类型筛选", hbox({
        tx_filter_toggle_->Render(),
        filler()
    }), tx_filter_toggle_->Focused(), PanelStyle::SubNav);

    Element left_box = panel(
        "多态交易流水列表",
        tx_list_->Render() | flex,
        tx_list_->Focused(),
        PanelStyle::Primary);

    const bool right_focused = go_mall_btn_->Focused() || go_cart_btn_->Focused();

    Element detail_content;
    if (!shown_txs_.empty() && tx_selected_ >= 0 &&
        tx_selected_ < static_cast<int>(shown_txs_.size())) {
        const auto* tx = shown_txs_[static_cast<std::size_t>(tx_selected_)];
        Elements extra_rows;
        if (const auto* rec = dynamic_cast<const user::RechargeRecord*>(tx)) {
            extra_rows.push_back(kv(" 充值渠道:", text(rec->getChannel()) | color(theme::Good)));
            extra_rows.push_back(kv(" 收支方向:", text("账户资金转入 (+)") | color(theme::Good)));
        } else if (const auto* exp = dynamic_cast<const user::ExpenseRecord*>(tx)) {
            extra_rows.push_back(kv(" 消费摘要:", exp->getDescription()));
            extra_rows.push_back(kv(" 收支方向:", text("订单结算支出 (-)") | color(theme::Warn)));
        } else if (const auto* pur = dynamic_cast<const user::PurchaseCouponRecord*>(tx)) {
            extra_rows.push_back(kv(" 所购卡券名:", text(pur->getCouponName()) | bold | color(theme::Accent)));
            extra_rows.push_back(kv(" 收支方向:", text("付费购券支出 (-)") | color(theme::Warn)));
        } else if (const auto* upg = dynamic_cast<const user::UpgradeRecord*>(tx)) {
            extra_rows.push_back(kv(" 变更前等级:", upg->getOldTierName()));
            extra_rows.push_back(kv(" 变更后等级:", text(upg->getNewTierName()) | bold | color(theme::Warn)));
        }

        detail_content = vbox({
            kv(" 交易类型:", text(tx->getTypeName()) | bold | color(theme::Accent)),
            kv(" 发生时间:", tx->getTimestamp()),
            kv(" 变动金额:", text(money(tx->getAmount())) | bold | color(theme::Good)),
            separator() | color(theme::Muted),
            vbox(std::move(extra_rows))
        });
    } else {
        detail_content = text(" 当前未选中账单流水记录") | color(theme::Muted);
    }

    Element right_box = panel(
        "交易流水凭证详情",
        vbox({
            std::move(detail_content),
            filler(),
            separator() | color(theme::Muted),
            hbox({go_mall_btn_->Render(), filler(), go_cart_btn_->Render()})
        }) | flex,
        right_focused,
        PanelStyle::Action) | size(WIDTH, EQUAL, 48);

    return vbox({
        std::move(filter_box),
        hbox({std::move(left_box) | flex, std::move(right_box)}) | flex
    }) | flex;
}

Element UserCenterPage::render() {
    syncData();
    const auto cust = ctx.user_mgr.getCurrentCustomer();
    if (!cust) {
        return text("请先登录消费者账号") | center;
    }

    Element sub_body;
    if (sub_tab_ == 0) {
        sub_body = renderAccountTab();
    } else if (sub_tab_ == 1) {
        sub_body = renderCouponBagTab();
    } else {
        sub_body = renderTransactionTab();
    }

    Element body = vbox({
        renderSubTabBar(),
        std::move(sub_body) | flex
    });

    return renderCustomerFrame(
        nav_bar_,
        std::move(body),
        "[ / ] 切换子页签 | Del 移除选中卡券 | C 清理已核销券 | Tab 切换焦点 | Esc 返回");
}

} // namespace ui
