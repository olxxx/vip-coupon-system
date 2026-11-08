#pragma once

#include "ui/Page.hpp"
#include <vector>

namespace ui {

class CheckoutPage : public Page {
private:
    int selected_idx_ = 0; // 0: 自动最优推荐, 1: 不使用优惠券, 2..N+1: 具体卡券
    bool paid_success_ = false;
    PaymentReceipt receipt_;
    std::vector<shop::CouponEvaluationItem> eval_items_;

    ftxui::Component nav_bar_;
    ftxui::Component scheme_list_;
    ftxui::Component pay_btn_;
    ftxui::Component go_recharge_btn_;
    ftxui::Component go_coupon_btn_;
    ftxui::Component back_btn_;

    void syncEvaluations();
    void executePayment();

public:
    explicit CheckoutPage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "收银台"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
