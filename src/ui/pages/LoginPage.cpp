#include "ui/pages/LoginPage.hpp"
#include "ui/pages/AdminPage.hpp"
#include "ui/pages/MallPage.hpp"

#include <ftxui/component/animation.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <cmath>

namespace ui {

using namespace ftxui;

namespace {

constexpr double kLogoRevealSec = 0.9;   // Logo 逐列扫描显现时长
constexpr double kTotalAnimSec  = 1.8;   // 整段入场动画时长
constexpr int    kShakeMs       = 500;   // 登录失败边框闪烁时长
constexpr int    kHeroMinWidth  = 112;   // 终端宽度达到此值才显示左侧品牌区

const std::vector<std::string> kLogoArt = {
    "██╗   ██╗██╗██████╗ ",
    "██║   ██║██║██╔══██╗",
    "██║   ██║██║██████╔╝",
    "╚██╗ ██╔╝██║██╔═══╝ ",
    " ╚████╔╝ ██║██║     ",
    "  ╚═══╝  ╚═╝╚═╝     ",
};

const std::vector<std::string> kFeatures = {
    "✦ 会员等级折扣自动叠加",
    "✦ 最优优惠券组合规划",
    "✦ 余额 / 流水 / 权益一站管理",
};

double clamp01(double v) { return std::clamp(v, 0.0, 1.0); }

Color fadeIn(double progress, Color target) {
    return Color::Interpolate(static_cast<float>(clamp01(progress)), theme::BadgeBg, target);
}

Element hintLine(bool show, const std::string& msg) {
    return text(show ? std::string(10, ' ') + msg : " ") | color(theme::Bad);
}

} // namespace

LoginPage::LoginPage(AppContext& context)
    : Page(context, "就绪。请选择角色并输入账号密码，或从下方演示账号一键填入。") {
    presets_ = {
        {"demo",  "123456",   "demo (普通用户)",    "消费者 V0",        false},
        {"alice", "123456",   "alice (白银会员)",   "白银 V2 · 95折",   false},
        {"root",  "admin123", "root (超级管理员)",  "全部管理权限",     true},
        {"staff", "staff123", "staff (运营管理员)", "商品与优惠券运营", true},
    };

    auto role_tog = InlineToggle(&role_labels_, &role_tab_);
    auto user_in  = TextInput(&username_, "输入用户名", false, [this] { doLogin(); });
    auto pass_msk = TextInput(&password_, "输入密码", true,  [this] { doLogin(); });
    auto pass_pln = TextInput(&password_, "输入密码", false, [this] { doLogin(); });
    auto pass_in  = Container::Tab({pass_msk, pass_pln}, &show_pw_idx_);

    auto eye_btn = StyledButton(
        [this] { return std::string(show_pw_idx_ ? "[隐藏]" : "[显示]"); },
        [this] { show_pw_idx_ = 1 - show_pw_idx_; },
        [] { return ButtonKind::Secondary; });

    auto login_btn = StyledButton(
        [] { return std::string("  登  录  "); },
        [this] { doLogin(); },
        [] { return ButtonKind::Primary; });

    auto reg_btn    = Maybe(ActionButton("注册新账号", [this] { doCustomerRegister(); }),
                            [this] { return role_tab_ == 0; });
    auto quit_btn   = ActionButton("退出系统", [this] { pending_action = PageAction::quit(); });

    auto pass_row = Container::Horizontal({pass_in, eye_btn});
    auto links    = Container::Horizontal({reg_btn, quit_btn});

    auto preset_list = DataList(
        &preset_sel_,
        [this] { return static_cast<int>(presets_.size()); },
        [this](int i, bool active, bool focused) {
            const auto& p = presets_[static_cast<std::size_t>(i)];
            Element row = hbox({
                text(active ? " ▸ " : "   "),
                col(p.label, 22),
                text(p.desc) | color(theme::Muted),
            });
            if (active) row = std::move(row) | color(theme::Accent);
            if (focused) row = std::move(row) | bold;
            return row;
        },
        [this] {
            fillPreset(presets_[static_cast<std::size_t>(preset_sel_)]);
            focus_idx_ = 3;  // 填充后焦点跳到登录按钮，Enter 即可登录
        });

    auto form = Container::Vertical({
        role_tog,
        user_in,
        pass_row,
        login_btn,
        links,
        preset_list,
    }, &focus_idx_);

    container = Renderer(form, [=, this] {
        const bool user_empty = attempted_ && trim(username_).empty();
        const bool pass_empty = attempted_ && password_.empty();

        Element login_box = login_btn->Render() | center
                            | borderStyled(ROUNDED) | color(theme::Accent);

        Element card_body = vbox({
            text("用户权益管理与优惠规划系统") | bold | color(Color::White) | hcenter,
            text("User Rights Management & Coupon Planning System") | color(theme::Muted) | hcenter,
            separator() | color(theme::Muted),
            hbox({col("  登录角色", 10) | color(Color::GrayLight), role_tog->Render()}),
            separatorEmpty(),
            hbox({col("  账号", 10) | color(Color::GrayLight), user_in->Render() | flex}),
            hintLine(user_empty, "请输入账号"),
            hbox({col("  密码", 10) | color(Color::GrayLight), pass_in->Render() | flex,
                  text(" "), eye_btn->Render()}),
            hintLine(pass_empty, "请输入密码"),
            login_box,
            links->Render() | hcenter,
            separator() | color(theme::Muted),
            text(" 演示账号 (↑/↓ 选择, Enter/单击填入):") | color(theme::Muted),
            preset_list->Render(),
        });

        // 登录失败时边框红白交替闪烁
        bool flash_red = false;
        if (isShaking()) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                shake_until_ - Clock::now()).count();
            flash_red = (left / 100) % 2 == 0;
            animation::RequestAnimationFrame();
        }
        Element card = panel("系统登录", std::move(card_body), true);
        if (flash_red) card = std::move(card) | color(theme::Bad);
        card = std::move(card) | size(WIDTH, EQUAL, 60);

        Elements cols;
        if (Terminal::Size().dimx >= kHeroMinWidth) {
            cols.push_back(renderHero() | size(WIDTH, EQUAL, 46));
            cols.push_back(text("    "));
        }
        cols.push_back(std::move(card));

        if (isAnimating()) animation::RequestAnimationFrame();

        return vbox({
            filler(),
            hbox(std::move(cols)) | center,
            filler(),
            renderFooterBar("Tab/方向键 切换 | Enter 登录 | Ctrl+E 显示密码 | F5/点击Logo 重播动画 | Esc 退出"),
        });
    });
}

double LoginPage::elapsed() const {
    return std::chrono::duration<double>(Clock::now() - anim_start_).count();
}

bool LoginPage::isAnimating() const { return elapsed() < kTotalAnimSec; }

bool LoginPage::isShaking() const { return Clock::now() < shake_until_; }

void LoginPage::replayAnimation() {
    anim_start_ = Clock::now();
    animation::RequestAnimationFrame();
}

Element LoginPage::renderLogo(double t) const {
    const double reveal = clamp01(t / kLogoRevealSec);
    const bool animating = t < kTotalAnimSec;
    const double amp = animating ? 1.0 - t / kTotalAnimSec : 0.0;
    const int width = 20;
    const double front = reveal * (width + 4);

    Elements rows;
    for (const auto& line : kLogoArt) {
        const auto glyphs = Utf8ToGlyphs(line);
        Elements cells;
        for (std::size_t c = 0; c < glyphs.size(); ++c) {
            const double dc = static_cast<double>(c);
            if (dc > front) {
                cells.push_back(text(" "));
                continue;
            }
            if (reveal < 1.0 && front - dc < 3.0) {
                cells.push_back(text(glyphs[c]) | bold | color(Color::White));
                continue;
            }
            double mix = dc / width + 0.3 * amp * std::sin(dc * 0.35 - t * 7.0);
            cells.push_back(text(glyphs[c]) | bold
                            | color(Color::Interpolate(static_cast<float>(clamp01(mix)),
                                                       theme::Accent, theme::Good)));
        }
        rows.push_back(hbox(std::move(cells)));
    }
    return vbox(std::move(rows));
}

Element LoginPage::renderHero() {
    const double t = elapsed();

    Elements lines;
    lines.push_back(renderLogo(t) | reflect(logo_box_));
    lines.push_back(text("  C O U P O N   S Y S T E M") | bold
                    | color(fadeIn((t - 0.5) / 0.3, theme::Accent)));
    lines.push_back(separatorEmpty());
    lines.push_back(text("  让每一笔消费都花在刀刃上")
                    | color(fadeIn((t - 0.7) / 0.3, Color::White)));
    lines.push_back(separatorEmpty());
    for (std::size_t i = 0; i < kFeatures.size(); ++i) {
        const double start = kLogoRevealSec + 0.2 * static_cast<double>(i);
        lines.push_back(text("  " + kFeatures[i])
                        | color(fadeIn((t - start) / 0.25, Color::GrayLight)));
    }
    lines.push_back(separatorEmpty());
    lines.push_back(text("  ▶ 点击 Logo 或按 F5 重播动画") | color(theme::Muted));

    return vbox(std::move(lines)) | borderStyled(ROUNDED) | color(theme::Muted);
}

void LoginPage::fillPreset(const Preset& p) {
    username_ = p.user;
    password_ = p.pass;
    role_tab_ = p.is_admin ? 1 : 0;
    attempted_ = false;
    setStatus("已填入预设账号 " + p.user + " (" + p.desc + ")，按 Enter 或点击登录。", false);
}

void LoginPage::doLogin() {
    attempted_ = true;
    if (trim(username_).empty() || password_.empty()) {
        setStatus("账号和密码均不能为空。", true);
        shake_until_ = Clock::now() + std::chrono::milliseconds(kShakeMs);
        return;
    }

    if (role_tab_ == 1) {
        auto res = ctx.loginAdmin(username_, password_);
        notify(res);
        if (res.success) {
            pending_action = PageAction::reset(std::make_unique<AdminPage>(ctx));
            return;
        }
    } else {
        auto res = ctx.loginCustomer(username_, password_);
        notify(res);
        if (res.success) {
            pending_action = PageAction::reset(std::make_unique<MallPage>(ctx, res.message));
            return;
        }
    }
    shake_until_ = Clock::now() + std::chrono::milliseconds(kShakeMs);
}

void LoginPage::doCustomerRegister() {
    attempted_ = true;
    auto res = ctx.registerCustomer(username_, password_);
    notify(res);
    if (!res.success) shake_until_ = Clock::now() + std::chrono::milliseconds(kShakeMs);
}

bool LoginPage::handle_event(Event event, PageAction& out_action) {
    if (event == Event::F5 || event == Event::Special("\x12")) {   // F5 / Ctrl+R
        replayAnimation();
        return true;
    }
    if (event == Event::Special("\x05")) {                          // Ctrl+E
        show_pw_idx_ = 1 - show_pw_idx_;
        return true;
    }
    if (event.is_mouse() && event.mouse().button == Mouse::Left &&
        event.mouse().motion == Mouse::Pressed &&
        logo_box_.Contain(event.mouse().x, event.mouse().y)) {
        replayAnimation();
        return true;
    }
    return Page::handle_event(event, out_action);
}

Element LoginPage::render() {
    return container->Render();
}

} // namespace ui
