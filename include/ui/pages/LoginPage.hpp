#pragma once

#include "ui/Page.hpp"
#include <string>

namespace ui {

class LoginPage : public Page {
private:
    std::string username_ = "demo";
    std::string password_ = "123456";

    void fillPreset(const std::string& user, const std::string& pass, const std::string& role_desc);
    void doCustomerLogin();
    void doCustomerRegister();
    void doAdminLogin();
    void doSmartSubmit();

public:
    explicit LoginPage(AppContext& ctx);
    std::string get_page_name() const override { return "系统登录"; }
    ftxui::Element render() override;
};

} // namespace ui
