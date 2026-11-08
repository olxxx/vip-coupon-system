#pragma once

#include "ui/Page.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ui {

class CouponSquarePage : public Page {
private:
    int filter_mode_ = 0; // 0: 全部, 1: 免费申领, 2: 付费购买, 3: 我可领
    int selected_idx_ = 0;
    std::vector<std::string> filter_labels_;
    std::vector<std::shared_ptr<shop::Coupon>> filtered_pool_;

    ftxui::Component nav_bar_;
    ftxui::Component filter_toggle_;
    ftxui::Component claim_all_btn_;
    ftxui::Component pool_list_;
    ftxui::Component acquire_btn_;
    ftxui::Component go_wallet_btn_;
    ftxui::Component go_cart_btn_;

    void syncPool();
    void handleAcquireSelected();
    void handleClaimAllFree();

public:
    explicit CouponSquarePage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "领券中心"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
