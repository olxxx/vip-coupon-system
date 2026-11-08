#include "ui/pages/LoginPage.hpp"
#include "ui/pages/AdminPage.hpp"
#include "ui/pages/MallPage.hpp"

namespace ui {

using namespace ftxui;

LoginPage::LoginPage(AppContext& context)
    : Page(context, "就绪。请输入账号密码登录，或点击下方预设账号一键填充。") {
    auto user_in = TextInput(&username_, "输入用户名", false, [this] { doSmartSubmit(); });
    auto pass_in = TextInput(&password_, "输入密码", true, [this] { doSmartSubmit(); });

    auto btn_cust_login = ActionButton("消费者登录", [this] { doCustomerLogin(); }, true);
    auto btn_cust_reg   = ActionButton("注册新账号", [this] { doCustomerRegister(); });
    auto btn_adm_login  = ActionButton("管理员登录", [this] { doAdminLogin(); });
    auto btn_quit       = ActionButton("退出系统", [this] { pending_action = PageAction::quit(); });

    auto btn_fill_demo  = ActionButton("demo (普通用户)", [this] { fillPreset("demo", "123456", "普通消费者 V0"); });
    auto btn_fill_alice = ActionButton("alice (白银会员)", [this] { fillPreset("alice", "123456", "白银会员 V2 · 95折"); });
    auto btn_fill_root  = ActionButton("root (超级管理员)", [this] { fillPreset("root", "admin123", "超级管理员"); });
    auto btn_fill_staff = ActionButton("staff (运营管理员)", [this] { fillPreset("staff", "staff123", "运营管理员"); });

    auto main_actions = Container::Horizontal({btn_cust_login, btn_cust_reg, btn_adm_login, btn_quit});
    auto preset_row1  = Container::Horizontal({btn_fill_demo, btn_fill_alice});
    auto preset_row2  = Container::Horizontal({btn_fill_root, btn_fill_staff});

    auto form = Container::Vertical({
        user_in,
        pass_in,
        main_actions,
        preset_row1,
        preset_row2
    });

    container = Renderer(form, [this, user_in, pass_in, main_actions, preset_row1, preset_row2] {
        Element card = panel("系统登录 (LoginPage)", vbox({
            vbox({
                text("用户权益管理与优惠规划系统") | bold | color(Color::White) | hcenter,
                text("User Rights Management & Coupon Planning System") | color(theme::Muted) | hcenter
            }),
            separator() | color(theme::Muted),
            vbox({
                hbox({
                    col("  登录账号: ", 14) | color(Color::GrayLight),
                    user_in->Render() | flex
                }),
                hbox({
                    col("  登录密码: ", 14) | color(Color::GrayLight),
                    pass_in->Render() | flex
                })
            }),
            separatorEmpty(),
            main_actions->Render() | hcenter,
            separator() | color(theme::Muted),
            text(" 快速填入预设账号 (方便课堂演示):") | color(theme::Muted),
            preset_row1->Render() | hcenter,
            preset_row2->Render() | hcenter
        }), true) | size(WIDTH, EQUAL, 74) | center;

        return vbox({
            filler(),
            std::move(card),
            filler(),
            renderFooterBar("Tab/方向键 切换控件 | Enter 确认登录 | Esc 退出系统")
        });
    });
}

void LoginPage::fillPreset(const std::string& user, const std::string& pass, const std::string& role_desc) {
    username_ = user;
    password_ = pass;
    setStatus("已填入预设账号 " + user + " (" + role_desc + ")，按 Enter 或点击对应登录按钮。", false);
}

void LoginPage::doCustomerLogin() {
    auto res = ctx.loginCustomer(username_, password_);
    notify(res);
    if (res.success) {
        pending_action = PageAction::reset(std::make_unique<MallPage>(ctx, res.message));
    }
}

void LoginPage::doCustomerRegister() {
    auto res = ctx.registerCustomer(username_, password_);
    notify(res);
}

void LoginPage::doAdminLogin() {
    auto res = ctx.loginAdmin(username_, password_);
    notify(res);
    if (res.success) {
        pending_action = PageAction::reset(std::make_unique<AdminPage>(ctx));
    }
}

void LoginPage::doSmartSubmit() {
    if (ctx.user_mgr.hasAdmin(trim(username_))) {
        doAdminLogin();
    } else {
        doCustomerLogin();
    }
}

Element LoginPage::render() {
    return container->Render();
}

} // namespace ui
