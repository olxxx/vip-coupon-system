#pragma once

#include "ui/Page.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ui {

class UserCenterPage : public Page {
private:
    std::string recharge_input_ = "500";
    int cpn_filter_ = 0; // 0: 全部, 1: 可使用, 2: 已核销
    int cpn_selected_ = 0;
    int tx_selected_ = 0;

    std::vector<std::string> cpn_tab_labels_;
    std::vector<std::shared_ptr<shop::Coupon>> shown_coupons_;
    std::vector<const user::TransactionRecord*> recent_txs_;

    ftxui::Component nav_bar_;
    ftxui::Component quick_100_btn_;
    ftxui::Component quick_500_btn_;
    ftxui::Component quick_1000_btn_;
    ftxui::Component custom_input_;
    ftxui::Component confirm_recharge_btn_;
    ftxui::Component cpn_filter_toggle_;
    ftxui::Component cpn_list_;
    ftxui::Component clean_used_btn_;
    ftxui::Component go_coupon_btn_;
    ftxui::Component tx_list_;
    ftxui::Component go_mall_btn_;
    ftxui::Component go_cart_btn_;

    void syncData();
    void handleCustomRecharge();

public:
    explicit UserCenterPage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "个人中心"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
