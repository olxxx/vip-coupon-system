#pragma once

#include "ui/Page.hpp"

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ui {

// 固定深度页面栈管理器 (严格对齐 DESIGN.md §10.3)
class PageManager : public ftxui::ComponentBase {
public:
    static constexpr std::size_t MAX_STACK_DEPTH = 8; // 固定历史栈容量上限

private:
    AppContext& ctx;
    std::function<void()> exit_closure;
    std::vector<std::unique_ptr<Page>> page_stack;

public:
    PageManager(AppContext& context, ftxui::ScreenInteractive& screen);
    PageManager(AppContext& context, std::function<void()> on_exit);

    // 压栈深入：若目标页面与当前栈顶同名则忽略；超出 MAX_STACK_DEPTH 自动淘汰栈底最旧页面
    void push_page(std::unique_ptr<Page> page);

    // 弹栈回退：栈深 > 1 时弹出栈顶返回上一页；栈底时若已登录则登出回 LoginPage，否则退出程序
    void pop_page();

    // 平级替换当前栈顶页面
    void change_page(std::unique_ptr<Page> page);

    // 登录或注销时清空并重置历史栈
    void reset_stack(std::unique_ptr<Page> root_page);

    // 查询状态 (供测试与顶栏展示)
    std::string get_previous_page_name() const;
    std::string get_current_page_name() const;
    std::size_t get_stack_depth() const { return page_stack.size(); }

    bool OnEvent(ftxui::Event event) override;
    ftxui::Element OnRender() override;
    bool Focusable() const override { return true; }

private:
    bool apply(PageAction action);
    void push_internal(std::unique_ptr<Page> page);
    void sync_context_stack_info();
};

} // namespace ui
