#pragma once

#include "ui/Page.hpp"
#include <vector>

namespace ui {

class CartPage : public Page {
private:
    int selected_idx_ = 0;
    std::vector<shop::CartItem> items_;

    ftxui::Component nav_bar_;
    ftxui::Component cart_list_;
    ftxui::Component inc_btn_;
    ftxui::Component dec_btn_;
    ftxui::Component remove_btn_;
    ftxui::Component clear_btn_;
    ftxui::Component checkout_btn_;
    ftxui::Component go_coupon_btn_;
    ftxui::Component go_mall_btn_;

    void syncItems();
    void modifySelectedQty(int delta);
    void removeSelected();
    void clearAll();
    void enterCheckout();

public:
    explicit CartPage(AppContext& ctx);
    void on_enter() override;
    std::string get_page_name() const override { return "购物车"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
