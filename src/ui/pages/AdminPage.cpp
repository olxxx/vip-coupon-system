#include "ui/pages/AdminPage.hpp"
#include "ui/pages/LoginPage.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace ui {

using namespace ftxui;

AdminPage::AdminPage(AppContext& context)
    : Page(context),
      prod_cat_labels_({"数码3C", "服饰", "食品", "日用", "其他"}),
      c_type_labels_({"满减券", "折扣券", "立减券"}),
      c_cat_labels_({"全场", "数码3C", "服饰", "食品", "日用", "其他"}),
      c_tier_labels_({"V0", "V1", "V2", "V3", "V4", "V5"}),
      c_time_labels_({"不限时间", "仅限工作日", "仅限周末"}),
      user_tier_labels_({"V0普通", "V1青铜", "V2白银", "V3黄金", "V4钻石", "V5至尊"}),
      admin_role_labels_({"运营管理员(Staff)", "超级管理员(Super)"}) {
    const auto adm = ctx.user_mgr.getCurrentAdmin();
    if (adm && adm->canModifyUserTier()) {
        setStatus("已登录为 " + adm->getUsername() + " (超级管理员)，拥有全部管理权限。", false);
    } else if (adm) {
        setStatus("已登录为 " + adm->getUsername() + " (运营管理员)，修改用户等级、删除用户或重置系统将被权限控制拦截。", false);
    }

    // 1. 顶部栏按钮
    auto make_tab_btn = [this](const std::string& label, int target_tab) {
        return StyledButton(
            [label] { return label; },
            [this, target_tab] {
                tab_ = target_tab;
                reset_armed_ = false;
                syncData();
            },
            [this, target_tab] { return tab_ == target_tab ? ButtonKind::TabActive : ButtonKind::Tab; });
    };

    auto tab0_btn = make_tab_btn("1.商品与库存管理", 0);
    auto tab1_btn = make_tab_btn("2.优惠券池发行", 1);
    auto tab2_btn = make_tab_btn("3.用户与管理员权限(RBAC)", 2);

    auto reset_click = [this] {
        const auto cur_adm = ctx.user_mgr.getCurrentAdmin();
        if (!cur_adm || !cur_adm->canResetSystem()) {
            notifyImportant(ctx.resetSystem());
            return;
        }
        if (!reset_armed_) {
            reset_armed_ = true;
            setStatus("请再点击一次「确认重置系统」 将全部数据恢复为初始演示数据！", false);
            return;
        }
        reset_armed_ = false;
        auto res = ctx.resetSystem();
        notifyImportant(res);
        if (res.success) {
            pending_action = PageAction::reset(std::make_unique<LoginPage>(ctx));
        }
    };
    auto reset_btn = StyledButton(
        [this] { return std::string(reset_armed_ ? "! 再次点击确认重置 !" : "重置系统默认数据"); },
        reset_click,
        [this] { return reset_armed_ ? ButtonKind::Danger : ButtonKind::Secondary; });

    auto logout_btn = ActionButton("退出后台(Esc)", [this] {
        ctx.logout();
        pending_action = PageAction::reset(std::make_unique<LoginPage>(ctx));
    });

    auto top_row = Container::Horizontal({tab0_btn, tab1_btn, tab2_btn, reset_btn, logout_btn});
    top_bar_ = Renderer(top_row, [=] {
        return hbox({
            filler(),
            tab0_btn->Render(),
            text("  "),
            tab1_btn->Render(),
            text("  "),
            tab2_btn->Render(),
            filler(),
            reset_btn->Render(),
            text(" "),
            logout_btn->Render()
        });
    });

    // 2. Tab 0: 商品与库存管理
    prod_list_ = DataList(
        &prod_idx_,
        [this] { return static_cast<int>(products_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& p = products_[static_cast<std::size_t>(idx)];
            return hbox({
                text(" "),
                col(p->getId(), 8) | color(theme::Muted),
                col(p->getName(), 20) | bold,
                col(p->getCategoryName(), 11) | color(Color::GrayLight),
                col(p->getFormattedPrice(), 11) | color(theme::Good),
                text("库存:" + std::to_string(p->getStock()))
                    | color(p->isOutOfStock() ? theme::Bad : Color::White)
            });
        },
        [this] {
            syncData();
            if (!products_.empty()) {
                const auto& p = products_[static_cast<std::size_t>(prod_idx_)];
                name_mod_ = p->getName();
                mod_cat_ = std::clamp(static_cast<int>(p->getCategory()) - 1, 0, 4);
            }
        });

    stock_input_ = TextInput(&stock_val_, "数量");
    replenish_btn_ = ActionButton("+补货", [this] {
        syncData();
        if (products_.empty()) return;
        int n = 0;
        if (!parseQuantity(stock_val_, n, false)) {
            setStatus("请输入大于 0 的整数补货数量", true);
            return;
        }
        notify(ctx.replenishProduct(products_[static_cast<std::size_t>(prod_idx_)]->getId(), n));
        syncData();
    });
    deduct_btn_ = ActionButton("-扣减", [this] {
        syncData();
        if (products_.empty()) return;
        int n = 0;
        if (!parseQuantity(stock_val_, n, false)) {
            setStatus("请输入大于 0 的整数扣减数量", true);
            return;
        }
        notify(ctx.deductProduct(products_[static_cast<std::size_t>(prod_idx_)]->getId(), n));
        syncData();
    });
    set_stock_btn_ = ActionButton("设库存", [this] {
        syncData();
        if (products_.empty()) return;
        int n = 0;
        if (!parseQuantity(stock_val_, n, true)) {
            setStatus("请输入非负整数库存数量", true);
            return;
        }
        notify(ctx.setProductStock(products_[static_cast<std::size_t>(prod_idx_)]->getId(), n));
        syncData();
    });
    del_prod_btn_ = DangerButton("下架", [this] {
        syncData();
        if (products_.empty()) return;
        notifyImportant(ctx.removeProduct(products_[static_cast<std::size_t>(prod_idx_)]->getId()));
        syncData();
    });

    price_input_ = TextInput(&price_mod_, "新单价");
    mod_price_btn_ = ActionButton("改单价", [this] {
        syncData();
        if (products_.empty()) return;
        double p = 0.0;
        if (!parseAmount(price_mod_, p, false)) {
            setStatus("请输入大于 0 的合法单价", true);
            return;
        }
        notify(ctx.updateProductPrice(products_[static_cast<std::size_t>(prod_idx_)]->getId(), p));
        syncData();
    });

    name_mod_input_ = TextInput(&name_mod_, "新商品名称");
    mod_name_btn_ = ActionButton("改名称", [this] {
        syncData();
        if (products_.empty()) return;
        notify(ctx.updateProductName(products_[static_cast<std::size_t>(prod_idx_)]->getId(), name_mod_));
        syncData();
    });

    mod_cat_toggle_ = InlineToggle(&prod_cat_labels_, &mod_cat_);
    mod_cat_btn_ = ActionButton("改分类", [this] {
        syncData();
        if (products_.empty()) return;
        const auto cat = static_cast<core::ProductCategory>(mod_cat_ + 1);
        notify(ctx.updateProductCategory(products_[static_cast<std::size_t>(prod_idx_)]->getId(), cat));
        syncData();
    });

    new_cat_toggle_  = InlineToggle(&prod_cat_labels_, &new_cat_);
    new_id_input_    = TextInput(&new_id_, "如P1004");
    new_name_input_  = TextInput(&new_name_, "商品名称");
    new_price_input_ = TextInput(&new_price_, "单价");
    new_stock_input_ = TextInput(&new_stock_, "库存");
    add_prod_btn_    = BigActionButton("+ 上架新商品至商城", [this] {
        double p = 0.0;
        int s = 0;
        if (!parseAmount(new_price_, p, false) || !parseQuantity(new_stock_, s, true)) {
            setStatus("上架失败：单价必须大于 0，库存需为非负整数", true);
            return;
        }
        const auto cat = static_cast<core::ProductCategory>(new_cat_ + 1);
        auto res = ctx.addProduct(new_id_, new_name_, cat, p, s);
        notifyImportant(res);
        syncData();
    }, ButtonKind::Primary);

    auto tab0_container = Container::Horizontal({
        prod_list_,
        Container::Vertical({
            Container::Horizontal({stock_input_, replenish_btn_, deduct_btn_, set_stock_btn_, del_prod_btn_}),
            Container::Horizontal({price_input_, mod_price_btn_, name_mod_input_, mod_name_btn_}),
            Container::Horizontal({mod_cat_toggle_, mod_cat_btn_}),
            new_cat_toggle_,
            Container::Horizontal({new_id_input_, new_name_input_}),
            Container::Horizontal({new_price_input_, new_stock_input_}),
            add_prod_btn_
        })
    });

    // 3. Tab 1: 优惠券池发行
    cpn_list_ = DataList(
        &cpn_idx_,
        [this] { return static_cast<int>(coupons_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& c = coupons_[static_cast<std::size_t>(idx)];
            return hbox({
                text(" "),
                col(c->getTemplateId(), 25) | color(theme::Muted),
                col(c->getName(), 20) | bold,
                text(c->getRuleDescription()) | color(theme::Good)
            });
        });

    del_cpn_btn_ = DangerButton("下架移除选中券模板", [this] {
        syncData();
        if (coupons_.empty()) return;
        notifyImportant(ctx.removeCoupon(coupons_[static_cast<std::size_t>(cpn_idx_)]->getTemplateId()));
        syncData();
    });

    c_type_toggle_ = InlineToggle(&c_type_labels_, &c_type_);
    c_cat_toggle_  = InlineToggle(&c_cat_labels_, &c_cat_);
    c_tier_toggle_ = InlineToggle(&c_tier_labels_, &c_tier_);
    c_time_toggle_ = InlineToggle(&c_time_labels_, &c_time_);

    c_id_input_   = TextInput(&c_id_, "模板ID");
    c_name_input_ = TextInput(&c_name_, "券名称");
    c_cost_input_ = TextInput(&c_cost_, "0为免费");
    auto c_min_raw = TextInput(&c_min_, "门槛额");
    c_min_input_  = Maybe(c_min_raw, [this] { return c_type_ != 2; });
    c_val_input_  = TextInput(&c_val_, "金额或折扣");
    auto c_max_raw = TextInput(&c_max_, "0不封顶");
    c_max_input_  = Maybe(c_max_raw, [this] { return c_type_ == 1; });

    add_cpn_btn_ = BigActionButton("+ 发布至公共领券中心", [this] {
        CouponDraft d;
        d.id = c_id_;
        d.name = c_name_;
        d.type = c_type_;
        d.category = static_cast<core::ProductCategory>(c_cat_);
        d.min_tier = static_cast<core::UserTier>(c_tier_);
        d.time_limit = static_cast<core::TimeConstraint>(c_time_);

        if (!parseAmount(c_cost_, d.cost, true) ||
            (c_type_ != 2 && !parseAmount(c_min_, d.min_spend, true)) ||
            !parseAmount(c_val_, d.value, false) ||
            (c_type_ == 1 && !parseAmount(c_max_, d.max_discount, true))) {
            setStatus("请填写有效的优惠券数值参数", true);
            return;
        }
        notifyImportant(ctx.issueCoupon(d));
        syncData();
    }, ButtonKind::Primary);

    auto tab1_container = Container::Horizontal({
        Container::Vertical({cpn_list_, del_cpn_btn_}),
        Container::Vertical({
            c_type_toggle_,
            c_cat_toggle_,
            c_tier_toggle_,
            c_time_toggle_,
            Container::Horizontal({c_id_input_, c_name_input_}),
            Container::Horizontal({c_cost_input_, c_min_input_}),
            Container::Horizontal({c_val_input_, c_max_input_}),
            add_cpn_btn_
        })
    });

    // 4. Tab 2: 用户等级与管理员权限管理 (RBAC)
    user_list_ = DataList(
        &user_idx_,
        [this] { return static_cast<int>(customers_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& u = customers_[static_cast<std::size_t>(idx)];
            return hbox({
                text(" "),
                col(u->getUsername(), 12) | bold,
                col(u->getTierName(), 12) | color(theme::Warn),
                col("余额:" + money(u->getBalance()), 18) | color(theme::Good),
                text("累计消费:" + money(u->getTotalSpent())) | color(Color::GrayLight)
            });
        });

    admin_list_ = DataList(
        &admin_idx_,
        [this] { return static_cast<int>(admins_.size()); },
        [this](int idx, bool /*active*/, bool /*focused*/) -> Element {
            const auto& a = admins_[static_cast<std::size_t>(idx)];
            const bool is_sup = (a->getRole() == core::AdminRole::SUPER_ADMIN);
            return hbox({
                text(" "),
                col(a->getUsername(), 12) | bold,
                col(a->getRoleName(), 16) | color(is_sup ? theme::Warn : theme::Accent),
                text("创建于:" + a->getCreatedAt()) | color(theme::Muted)
            });
        });

    target_tier_toggle_ = InlineToggle(&user_tier_labels_, &target_tier_);
    mod_tier_btn_ = ActionButton("执行等级修改", [this] {
        syncData();
        if (customers_.empty()) return;
        const auto uname = customers_[static_cast<std::size_t>(user_idx_)]->getUsername();
        notifyImportant(ctx.updateCustomerTier(uname, static_cast<core::UserTier>(target_tier_)));
        syncData();
    }, true);

    user_bal_input_ = TextInput(&user_bal_mod_, "设定余额");
    set_bal_btn_ = ActionButton("设定余额", [this] {
        syncData();
        if (customers_.empty()) return;
        double b = 0.0;
        if (!parseAmount(user_bal_mod_, b, true)) {
            setStatus("请输入非负合法金额", true);
            return;
        }
        const auto uname = customers_[static_cast<std::size_t>(user_idx_)]->getUsername();
        notifyImportant(ctx.setCustomerBalance(uname, b));
        syncData();
    });

    del_user_btn_ = DangerButton("删除选中消费者账号", [this] {
        syncData();
        if (customers_.empty()) return;
        const auto uname = customers_[static_cast<std::size_t>(user_idx_)]->getUsername();
        notifyImportant(ctx.removeCustomer(uname));
        syncData();
    });

    admin_role_toggle_ = InlineToggle(&admin_role_labels_, &admin_role_sel_);
    mod_adm_role_btn_ = ActionButton("修改角色", [this] {
        syncData();
        if (admins_.empty()) return;
        const auto aname = admins_[static_cast<std::size_t>(admin_idx_)]->getUsername();
        const auto role = (admin_role_sel_ == 1) ? core::AdminRole::SUPER_ADMIN : core::AdminRole::STAFF;
        notifyImportant(ctx.updateAdminRole(aname, role));
        syncData();
    });

    del_adm_btn_ = DangerButton("删除管理员", [this] {
        syncData();
        if (admins_.empty()) return;
        const auto aname = admins_[static_cast<std::size_t>(admin_idx_)]->getUsername();
        notifyImportant(ctx.removeAdmin(aname));
        syncData();
    });

    new_adm_user_input_ = TextInput(&new_adm_user_, "新管理员账号");
    new_adm_pwd_input_  = TextInput(&new_adm_pwd_, "登录密码", true);
    add_adm_btn_ = ActionButton("+新增管理员", [this] {
        const auto role = (admin_role_sel_ == 1) ? core::AdminRole::SUPER_ADMIN : core::AdminRole::STAFF;
        auto res = ctx.registerAdmin(new_adm_user_, new_adm_pwd_, role);
        notifyImportant(res);
        if (res.success) {
            new_adm_user_.clear();
            new_adm_pwd_.clear();
        }
        syncData();
    }, true);

    auto tab2_container = Container::Horizontal({
        Container::Vertical({user_list_, admin_list_}),
        Container::Vertical({
            target_tier_toggle_,
            Container::Horizontal({mod_tier_btn_, del_user_btn_}),
            Container::Horizontal({user_bal_input_, set_bal_btn_}),
            admin_role_toggle_,
            Container::Horizontal({mod_adm_role_btn_, del_adm_btn_}),
            Container::Horizontal({new_adm_user_input_, new_adm_pwd_input_, add_adm_btn_})
        })
    });

    tab_container_ = Container::Tab({tab0_container, tab1_container, tab2_container}, &tab_);
    container = Container::Vertical({top_bar_, tab_container_});

    syncData();
    prod_list_->TakeFocus();
}

void AdminPage::on_enter() {
    syncData();
}

void AdminPage::syncData() {
    products_ = ctx.prod_mgr.getProducts();
    if (!products_.empty()) {
        prod_idx_ = std::clamp(prod_idx_, 0, static_cast<int>(products_.size()) - 1);
    } else {
        prod_idx_ = 0;
    }

    coupons_ = ctx.cpn_mgr.getPublicPool();
    if (!coupons_.empty()) {
        cpn_idx_ = std::clamp(cpn_idx_, 0, static_cast<int>(coupons_.size()) - 1);
    } else {
        cpn_idx_ = 0;
    }

    customers_ = ctx.user_mgr.getAllCustomers();
    if (!customers_.empty()) {
        user_idx_ = std::clamp(user_idx_, 0, static_cast<int>(customers_.size()) - 1);
    } else {
        user_idx_ = 0;
    }

    admins_ = ctx.user_mgr.getAllAdmins();
    if (!admins_.empty()) {
        admin_idx_ = std::clamp(admin_idx_, 0, static_cast<int>(admins_.size()) - 1);
    } else {
        admin_idx_ = 0;
    }
}

bool AdminPage::handle_event(Event event, PageAction& out_action) {
    bool handled = container->OnEvent(event);
    if (!handled) {
        if (event == Event::Character('1')) {
            tab_ = 0;
            reset_armed_ = false;
            handled = true;
        } else if (event == Event::Character('2')) {
            tab_ = 1;
            reset_armed_ = false;
            handled = true;
        } else if (event == Event::Character('3')) {
            tab_ = 2;
            reset_armed_ = false;
            handled = true;
        } else if (event == Event::Escape) {
            ctx.logout();
            pending_action = PageAction::reset(std::make_unique<LoginPage>(ctx));
            handled = true;
        }
    }
    out_action = std::move(pending_action);
    pending_action = PageAction::none();
    return handled || (out_action.type != PageAction::Type::None);
}

Element AdminPage::renderAdminHeader() {
    const auto adm = ctx.user_mgr.getCurrentAdmin();
    const std::string adm_str = adm
        ? (adm->getUsername() + " [" + adm->getRoleName() + "]")
        : "未登录";

    return panel("后台运营控制台 (AdminPage)", hbox({
        text(" 管理员: ") | color(theme::Muted),
        text(adm_str) | bold | color(theme::Warn),
        top_bar_->Render() | flex
    }), top_bar_->Focused(), PanelStyle::Header);
}

Element AdminPage::renderProductTab() {
    std::string sel_name = "无";
    if (!products_.empty()) {
        sel_name = products_[static_cast<std::size_t>(prod_idx_)]->getName();
    }

    Element left_box = panel("系统商品列表", prod_list_->Render() | flex, prod_list_->Focused(), PanelStyle::Primary);

    const bool maint_focused = stock_input_->Focused() || replenish_btn_->Focused() ||
                               deduct_btn_->Focused() || set_stock_btn_->Focused() ||
                               del_prod_btn_->Focused() || price_input_->Focused() ||
                               mod_price_btn_->Focused() || name_mod_input_->Focused() ||
                               mod_name_btn_->Focused() || mod_cat_toggle_->Focused() ||
                               mod_cat_btn_->Focused();

    Element maint_box = panel("维护选中商品: " + sel_name, vbox({
        hbox({
            text("库存: ") | color(theme::Muted),
            stock_input_->Render() | size(WIDTH, EQUAL, 7),
            text(" "),
            replenish_btn_->Render(),
            text(" "),
            deduct_btn_->Render(),
            text(" "),
            set_stock_btn_->Render(),
            filler(),
            del_prod_btn_->Render()
        }),
        hbox({
            text("单价: ") | color(theme::Muted),
            price_input_->Render() | size(WIDTH, EQUAL, 9),
            text(" "),
            mod_price_btn_->Render(),
            text("  名称: ") | color(theme::Muted),
            name_mod_input_->Render() | flex,
            text(" "),
            mod_name_btn_->Render()
        }),
        hbox({
            text("分类:") | color(theme::Muted),
            mod_cat_toggle_->Render() | flex,
            mod_cat_btn_->Render()
        })
    }), maint_focused, PanelStyle::Action);

    const bool add_focused = new_cat_toggle_->Focused() || new_id_input_->Focused() ||
                             new_name_input_->Focused() || new_price_input_->Focused() ||
                             new_stock_input_->Focused() || add_prod_btn_->Focused();

    Element add_box = panel("上架新商品", vbox({
        hbox({text("分类:") | color(theme::Muted), new_cat_toggle_->Render()}),
        hbox({
            text("编号: ") | color(theme::Muted),
            new_id_input_->Render() | size(WIDTH, EQUAL, 12),
            text("  名称: ") | color(theme::Muted),
            new_name_input_->Render() | flex
        }),
        hbox({
            text("单价: ") | color(theme::Muted),
            new_price_input_->Render() | size(WIDTH, EQUAL, 12),
            text("  库存: ") | color(theme::Muted),
            new_stock_input_->Render() | flex
        }),
        filler(),
        add_prod_btn_->Render()
    }) | flex, add_focused, PanelStyle::Action);

    return hbox({
        std::move(left_box) | flex,
        vbox({std::move(maint_box), std::move(add_box) | flex}) | size(WIDTH, EQUAL, 58)
    }) | flex;
}

Element AdminPage::renderCouponTab() {
    Element left_box = panel(
        "公共池优惠券模板",
        vbox({
            cpn_list_->Render() | flex,
            separator() | color(theme::Muted),
            del_cpn_btn_->Render() | hcenter
        }),
        cpn_list_->Focused() || del_cpn_btn_->Focused(),
        PanelStyle::Primary);

    const bool form_focused = c_type_toggle_->Focused() || c_cat_toggle_->Focused() ||
                              c_tier_toggle_->Focused() || c_time_toggle_->Focused() ||
                              c_id_input_->Focused() || c_name_input_->Focused() ||
                              c_cost_input_->Focused() || c_min_input_->Focused() ||
                              c_val_input_->Focused() || c_max_input_->Focused() ||
                              add_cpn_btn_->Focused();

    const std::string val_label = (c_type_ == 1) ? "折扣(0~1): " : "减免额: ";

    Element right_box = panel(
        "发布新优惠券模板 (四维约束配置)",
        vbox({
            hbox({text("券种:") | color(theme::Muted), c_type_toggle_->Render()}),
            hbox({text("品类:") | color(theme::Muted), c_cat_toggle_->Render()}),
            hbox({text("等级:") | color(theme::Muted), c_tier_toggle_->Render()}),
            hbox({text("时间:") | color(theme::Muted), c_time_toggle_->Render()}),
            separator() | color(theme::Muted),
            hbox({
                text("模板ID: ") | color(theme::Muted),
                c_id_input_->Render() | size(WIDTH, EQUAL, 16),
                text("  名称: ") | color(theme::Muted),
                c_name_input_->Render() | flex
            }),
            hbox({
                text("购券费: ") | color(theme::Muted),
                c_cost_input_->Render() | size(WIDTH, EQUAL, 14),
                c_type_ != 2 ? (text("  门槛额: ") | color(theme::Muted)) : emptyElement(),
                c_min_input_->Render() | flex
            }),
            hbox({
                text(val_label) | color(theme::Muted),
                c_val_input_->Render() | size(WIDTH, EQUAL, 14),
                c_type_ == 1 ? (text("  封顶额: ") | color(theme::Muted)) : emptyElement(),
                c_max_input_->Render() | flex
            }),
            filler(),
            add_cpn_btn_->Render()
        }) | flex,
        form_focused,
        PanelStyle::Action) | size(WIDTH, EQUAL, 58);

    return hbox({std::move(left_box) | flex, std::move(right_box)}) | flex;
}

Element AdminPage::renderUserTab() {
    Element cust_list_box = panel("注册消费者列表", user_list_->Render() | flex, user_list_->Focused(), PanelStyle::Primary);
    Element adm_list_box  = panel("后台管理员列表", admin_list_->Render() | flex, admin_list_->Focused(), PanelStyle::Primary)
                            | size(HEIGHT, EQUAL, 8);

    const auto adm = ctx.user_mgr.getCurrentAdmin();
    const bool is_super = adm && adm->canModifyUserTier();
    const std::string role_desc = adm
        ? (adm->getRoleName() + (is_super ? " (允许调级/删号)" : " (仅只读查看)"))
        : "未登录";

    const bool cust_ops_focused = target_tier_toggle_->Focused() || mod_tier_btn_->Focused() ||
                                  user_bal_input_->Focused() || set_bal_btn_->Focused() ||
                                  del_user_btn_->Focused();

    Element cust_ops_content;
    if (!customers_.empty()) {
        const auto& sel_u = customers_[static_cast<std::size_t>(user_idx_)];
        cust_ops_content = vbox({
            hbox({
                text("选中消费者: ") | color(theme::Muted),
                text(sel_u->getUsername()) | bold | color(Color::White),
                text(" (") | color(theme::Muted),
                text(sel_u->getTierName()) | bold | color(theme::Warn),
                text(" | 余额 " + money(sel_u->getBalance()) + ")") | color(theme::Good)
            }),
            hbox({
                text("当前权限: ") | color(theme::Muted),
                text(role_desc) | bold | color(is_super ? theme::Good : theme::Bad)
            }),
            target_tier_toggle_->Render(),
            hbox({
                mod_tier_btn_->Render(),
                filler(),
                del_user_btn_->Render()
            }),
            hbox({
                text("设定余额: ") | color(theme::Muted),
                user_bal_input_->Render() | size(WIDTH, EQUAL, 14),
                filler(),
                set_bal_btn_->Render()
            })
        });
    } else {
        cust_ops_content = text("暂无消费者账号") | color(theme::Muted);
    }

    Element cust_ops_box = panel("消费者账户调控 (RBAC)", std::move(cust_ops_content), cust_ops_focused, PanelStyle::Action);

    const bool adm_ops_focused = admin_role_toggle_->Focused() || mod_adm_role_btn_->Focused() ||
                                 del_adm_btn_->Focused() || new_adm_user_input_->Focused() ||
                                 new_adm_pwd_input_->Focused() || add_adm_btn_->Focused();

    std::string sel_adm_name = admins_.empty()
        ? "无"
        : (admins_[static_cast<std::size_t>(admin_idx_)]->getUsername() + " [" +
           admins_[static_cast<std::size_t>(admin_idx_)]->getRoleName() + "]");

    Element adm_ops_box = panel("管理员账号管理 (RBAC)", vbox({
        kv("选中管理员:", text(sel_adm_name) | bold | color(theme::Accent)),
        hbox({text("角色:") | color(theme::Muted), admin_role_toggle_->Render()}),
        hbox({mod_adm_role_btn_->Render(), filler(), del_adm_btn_->Render()}),
        separator() | color(theme::Muted),
        hbox({
            text("新账号:") | color(theme::Muted),
            new_adm_user_input_->Render() | size(WIDTH, EQUAL, 12),
            text(" 密码:") | color(theme::Muted),
            new_adm_pwd_input_->Render() | flex,
            text(" "),
            add_adm_btn_->Render()
        })
    }) | flex, adm_ops_focused, PanelStyle::Action);

    return hbox({
        vbox({std::move(cust_list_box) | flex, std::move(adm_list_box)}) | flex,
        vbox({std::move(cust_ops_box), std::move(adm_ops_box) | flex}) | size(WIDTH, EQUAL, 58)
    }) | flex;
}

Element AdminPage::render() {
    syncData();
    Element tab_body;
    if (tab_ == 0) tab_body = renderProductTab();
    else if (tab_ == 1) tab_body = renderCouponTab();
    else tab_body = renderUserTab();

    return vbox({
        renderAdminHeader(),
        std::move(tab_body) | flex,
        renderFooterBar("1/2/3 切换管理模块 | Tab/方向键 切换控件 | Esc 退出后台")
    });
}

} // namespace ui
