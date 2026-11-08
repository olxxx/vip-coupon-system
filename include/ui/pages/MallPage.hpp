#pragma once

#include "ui/Page.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ui {

class MallPage : public Page {
private:
    int cat_filter_ = 0; // 0: 全部, 1..5: 对应品类
    std::string search_kw_;
    int selected_idx_ = 0;
    int add_qty_ = 1;

    std::vector<std::string> cat_labels_;
    std::vector<std::shared_ptr<shop::Product>> filtered_products_;

    ftxui::Component nav_bar_;
    ftxui::Component cat_toggle_;
    ftxui::Component search_input_;
    ftxui::Component clear_search_btn_;
    ftxui::Component product_list_;
    ftxui::Component qty_dec_btn_;
    ftxui::Component qty_inc_btn_;
    ftxui::Component add_cart_btn_;
    ftxui::Component go_cart_btn_;
    ftxui::Component go_coupon_btn_;

    void refreshFilter();
    void handleAddToCart();

public:
    explicit MallPage(AppContext& ctx, std::string welcome_msg = "");
    void on_enter() override;
    std::string get_page_name() const override { return "商城货架"; }
    bool handle_event(ftxui::Event event, PageAction& out_action) override;
    ftxui::Element render() override;
};

} // namespace ui
