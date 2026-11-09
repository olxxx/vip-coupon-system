#pragma once

#include "ui/Page.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace ui {

class LoginPage : public Page {
private:
    using Clock = std::chrono::steady_clock;

    struct Preset {
        std::string user;
        std::string pass;
        std::string label;   // 如 "alice (白银会员)"
        std::string desc;    // 权益说明
        bool is_admin;
    };

    std::string username_;
    std::string password_;

    int role_tab_ = 0;          // 0 = 消费者, 1 = 管理员
    int show_pw_idx_ = 0;       // 0 = 掩码, 1 = 明文 (对应 Container::Tab 的活动页)
    int focus_idx_ = 1;         // 表单容器初始焦点：账号输入框
    int preset_sel_ = 0;
    bool attempted_ = false;    // 是否已尝试提交 (用于显示即时校验提示)

    std::vector<std::string> role_labels_{"消费者", "管理员"};
    std::vector<Preset> presets_;

    Clock::time_point anim_start_ = Clock::now();
    Clock::time_point shake_until_{};
    ftxui::Box logo_box_;

    void fillPreset(const Preset& p);
    void doLogin();
    void doCustomerRegister();
    void replayAnimation();

    double elapsed() const;
    bool isAnimating() const;
    bool isShaking() const;
    ftxui::Element renderLogo(double t) const;
    ftxui::Element renderHero();

public:
    explicit LoginPage(AppContext& ctx);
    std::string get_page_name() const override { return "系统登录"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
