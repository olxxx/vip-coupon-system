#pragma once

#include "ui/Page.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ui {

class AdminPage : public Page {
private:
    int tab_ = 0; // 0: 商品与库存管理, 1: 优惠券池发行, 2: 用户等级管理(RBAC)
    bool reset_armed_ = false; // 连按两次确认重置系统

    // Tab 0: 商品管理状态
    int prod_idx_ = 0;
    std::string stock_val_ = "20";
    std::string price_mod_ = "399.00";
    std::string name_mod_;
    int mod_cat_ = 0; // 0..4 对应 DIGITAL..OTHER
    int new_cat_ = 0; // 0..4 对应 DIGITAL..OTHER
    std::string new_id_ = "P1004";
    std::string new_name_ = "无线鼠标";
    std::string new_price_ = "159";
    std::string new_stock_ = "40";
    std::vector<std::string> prod_cat_labels_;
    std::vector<std::shared_ptr<shop::Product>> products_;

    // Tab 1: 优惠券发行状态
    int cpn_idx_ = 0;
    int c_type_ = 0; // 0: 满减, 1: 折扣, 2: 立减
    int c_cat_ = 0;  // 0: 全场, 1..5: 品类
    int c_tier_ = 0; // 0..5: V0..V5
    int c_time_ = 0; // 0: 不限, 1: 工作日, 2: 周末
    std::string c_id_ = "TPL_NEW_50";
    std::string c_name_ = "店庆满300减50";
    std::string c_cost_ = "0";
    std::string c_min_ = "300";
    std::string c_val_ = "50";
    std::string c_max_ = "0";
    std::vector<std::string> c_type_labels_;
    std::vector<std::string> c_cat_labels_;
    std::vector<std::string> c_tier_labels_;
    std::vector<std::string> c_time_labels_;
    std::vector<std::shared_ptr<shop::Coupon>> coupons_;

    // Tab 2: 用户与管理员管理状态
    int user_idx_ = 0;
    int target_tier_ = 2;
    std::string user_bal_mod_ = "1000";
    int admin_idx_ = 0;
    int admin_role_sel_ = 0; // 0: STAFF, 1: SUPER_ADMIN
    std::string new_adm_user_;
    std::string new_adm_pwd_;
    std::vector<std::string> user_tier_labels_;
    std::vector<std::string> admin_role_labels_;
    std::vector<std::shared_ptr<user::CustomerAccount>> customers_;
    std::vector<std::shared_ptr<user::AdminAccount>> admins_;

    // 顶栏与各 Tab 组件
    ftxui::Component top_bar_;
    ftxui::Component tab_container_;

    // Tab 0 组件
    ftxui::Component prod_list_;
    ftxui::Component stock_input_;
    ftxui::Component replenish_btn_;
    ftxui::Component deduct_btn_;
    ftxui::Component set_stock_btn_;
    ftxui::Component del_prod_btn_;
    ftxui::Component price_input_;
    ftxui::Component mod_price_btn_;
    ftxui::Component name_mod_input_;
    ftxui::Component mod_name_btn_;
    ftxui::Component mod_cat_toggle_;
    ftxui::Component mod_cat_btn_;
    ftxui::Component new_cat_toggle_;
    ftxui::Component new_id_input_;
    ftxui::Component new_name_input_;
    ftxui::Component new_price_input_;
    ftxui::Component new_stock_input_;
    ftxui::Component add_prod_btn_;

    // Tab 1 组件
    ftxui::Component cpn_list_;
    ftxui::Component del_cpn_btn_;
    ftxui::Component c_type_toggle_;
    ftxui::Component c_cat_toggle_;
    ftxui::Component c_tier_toggle_;
    ftxui::Component c_time_toggle_;
    ftxui::Component c_id_input_;
    ftxui::Component c_name_input_;
    ftxui::Component c_cost_input_;
    ftxui::Component c_min_input_;
    ftxui::Component c_val_input_;
    ftxui::Component c_max_input_;
    ftxui::Component add_cpn_btn_;

    // Tab 2 组件
    ftxui::Component user_list_;
    ftxui::Component admin_list_;
    ftxui::Component target_tier_toggle_;
    ftxui::Component mod_tier_btn_;
    ftxui::Component user_bal_input_;
    ftxui::Component set_bal_btn_;
    ftxui::Component del_user_btn_;
    ftxui::Component admin_role_toggle_;
    ftxui::Component mod_adm_role_btn_;
    ftxui::Component del_adm_btn_;
    ftxui::Component new_adm_user_input_;
    ftxui::Component new_adm_pwd_input_;
    ftxui::Component add_adm_btn_;

    void syncData();
    ftxui::Element renderAdminHeader();
    ftxui::Element renderProductTab();
    ftxui::Element renderCouponTab();
    ftxui::Element renderUserTab();

public:
    explicit AdminPage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "后台运营控制台"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
