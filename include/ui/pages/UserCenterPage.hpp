#pragma once

#include "ui/Page.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ui {

class UserCenterPage : public Page {
private:
    int sub_tab_ = 0; // 0: 账户充值与安全, 1: 我的卡券背包, 2: 账单流水历史

    // Tab 0: 充值与修改密码
    std::string recharge_input_ = "500";
    std::string old_pwd_input_;
    std::string new_pwd_input_;

    // Tab 1: 卡券背包
    int cpn_filter_ = 0; // 0: 全部, 1: 可使用, 2: 已核销
    int cpn_selected_ = 0;
    std::vector<std::string> cpn_tab_labels_;
    std::vector<std::shared_ptr<shop::Coupon>> shown_coupons_;

    // Tab 2: 账单流水
    int tx_filter_ = 0; // 0: 全部, 1: 充值, 2: 消费, 3: 购券, 4: 升级
    int tx_selected_ = 0;
    std::vector<std::string> tx_filter_labels_;
    std::vector<const user::TransactionRecord*> shown_txs_;

    // 公共与子页签栏
    ftxui::Component nav_bar_;
    ftxui::Component sub_tab_bar_;
    ftxui::Component sub_tab_container_;

    // Tab 0 组件
    ftxui::Component quick_100_btn_;
    ftxui::Component quick_500_btn_;
    ftxui::Component quick_1000_btn_;
    ftxui::Component custom_input_;
    ftxui::Component confirm_recharge_btn_;
    ftxui::Component old_pwd_comp_;
    ftxui::Component new_pwd_comp_;
    ftxui::Component change_pwd_btn_;

    // Tab 1 组件
    ftxui::Component cpn_filter_toggle_;
    ftxui::Component cpn_list_;
    ftxui::Component del_cpn_btn_;
    ftxui::Component clean_used_btn_;
    ftxui::Component clear_cpn_btn_;
    ftxui::Component go_coupon_btn_;

    // Tab 2 组件
    ftxui::Component tx_filter_toggle_;
    ftxui::Component tx_list_;
    ftxui::Component go_mall_btn_;
    ftxui::Component go_cart_btn_;

    void syncData();
    void handleCustomRecharge();
    void handleChangePassword();
    void handleRemoveSelectedCoupon();
    void handleCleanUsedCoupons();
    void handleClearAllCoupons();

    ftxui::Element renderSubTabBar();
    ftxui::Element renderAccountTab();
    ftxui::Element renderCouponBagTab();
    ftxui::Element renderTransactionTab();

public:
    explicit UserCenterPage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "个人中心"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
