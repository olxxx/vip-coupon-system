#pragma once
#include "ui/AppContext.hpp"
#include <ftxui/component/component_base.hpp>
#include <functional>
#include <memory>

namespace ui {
enum class Page { Login, Mall, Cart, Checkout, UserCenter, CouponSquare, Admin };
class Application {
public:
    Application(AppContext& context, std::function<void()> exit);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ftxui::Component component() const;
    Page page() const;
    void navigate(Page page);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ui
