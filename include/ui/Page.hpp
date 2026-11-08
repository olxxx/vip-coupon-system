#pragma once

#include "ui/AppContext.hpp"
#include "ui/Theme.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <memory>
#include <string>
#include <utility>

namespace ui {

class Page;

// 页面状态机跳转指令 (严格对齐 DESIGN.md §10.2)
struct PageAction {
    enum class Type {
        None,   // 停留在当前页
        Pop,    // 弹出栈顶，返回上一页历史 (Esc)
        Push,   // 深入子页面 (推入新页面到栈顶)
        Change, // 替换当前栈顶页面
        Reset,  // 重置并清空历史栈，进入全新根页面 (登录/登出)
        Quit    // 退出程序
    };

    Type type = Type::None;
    std::unique_ptr<Page> next_page = nullptr;

    static PageAction none() { return {Type::None, nullptr}; }
    static PageAction pop() { return {Type::Pop, nullptr}; }
    static PageAction push(std::unique_ptr<Page> p) { return {Type::Push, std::move(p)}; }
    static PageAction change(std::unique_ptr<Page> p) { return {Type::Change, std::move(p)}; }
    static PageAction reset(std::unique_ptr<Page> p) { return {Type::Reset, std::move(p)}; }
    static PageAction quit() { return {Type::Quit, nullptr}; }
};

// 页面抽象基类 (内建 FTXUI 交互容器、事件分发管道与消费者公共导航栏)
class Page {
protected:
    AppContext& ctx;
    ftxui::Component container = ftxui::Container::Vertical({});
    PageAction pending_action = PageAction::none();
    std::string status_msg;
    bool is_error = false;

public:
    explicit Page(AppContext& context, std::string init_msg = "")
        : ctx(context), status_msg(std::move(init_msg)) {}

    virtual ~Page() = default;

    virtual void on_enter() {}
    virtual void on_exit() {}
    virtual std::string get_page_name() const = 0;

    // 核心事件分发机制：
    // 1. 优先交由页面内挂载的 FTXUI 组件消费（如输入框打字、列表方向键、按钮回车）；
    // 2. 若子组件未消费且按下了 Esc，则触发 Pop 回退；
    // 3. 将挂起的跳转动作传出给 PageManager。
    virtual bool handle_event(ftxui::Event event, PageAction& out_action);

    virtual ftxui::Element render() {
        return container->Render();
    }

protected:
    // 更新底部状态栏消息
    void notify(const core::Result& res) {
        status_msg = res.message;
        is_error = !res.success;
    }

    void setStatus(std::string msg, bool err = false) {
        status_msg = std::move(msg);
        is_error = err;
    }

    // 构建消费者端顶部导航按钮组 (active_tab: 1=商城, 2=购物车, 3=领券, 4=个人中心, 0=其他如收银台)
    ftxui::Component buildCustomerNavBar(int active_tab);

    // 消费者端全局快捷键处理 (数字键 1-4 压栈切页，T 切换工作日/周末)
    bool handleCustomerShortcuts(const ftxui::Event& event, int active_tab);

    // 渲染消费者端公共顶栏 (资产摘要 + 导航按钮栏)
    ftxui::Element renderCustomerHeader(const ftxui::Component& nav_bar) const;

    // 渲染底部单行状态与快捷键提示栏
    ftxui::Element renderFooterBar(const std::string& key_hints) const;

    // 组装消费者标准全屏页面框架 (顶栏 + 页面主体 + 底栏)
    ftxui::Element renderCustomerFrame(const ftxui::Component& nav_bar,
                                       ftxui::Element body,
                                       const std::string& key_hints) const;
};

} // namespace ui
