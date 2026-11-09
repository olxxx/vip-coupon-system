#pragma once

#include "ui/AppContext.hpp"
#include "ui/Theme.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
    virtual void on_exit() { popup_.open = false; }
    virtual std::string get_page_name() const = 0;

    // 核心事件分发机制：
    // 1. 优先交由页面内挂载的 FTXUI 组件消费（如输入框打字、列表方向键、按钮回车）；
    // 2. 若子组件未消费且按下了 Esc，则触发 Pop 回退；
    // 3. 将挂起的跳转动作传出给 PageManager。
    virtual bool handle_event(ftxui::Event event, PageAction& out_action);

    virtual ftxui::Element render() {
        return container->Render();
    }

    // 弹窗：由 PageManager 在分发事件前检查，打开时独占全部按键
    bool hasPopup() const { return popup_.open; }
    void handlePopupEvent(ftxui::Event event, PageAction& out_action);
    // 渲染页面；弹窗打开时叠加在上层
    ftxui::Element renderWithPopup();

private:
    struct PopupNav {
        std::string label;
        std::function<std::unique_ptr<Page>()> make;
    };
    struct Popup {
        bool open = false;
        bool error = false;
        std::string title;
        std::string msg;
        std::vector<PopupNav> navs; // 推荐导航；末尾追加的"确定"按钮不在其中
        int cursor = 0;             // 0..navs.size()，navs.size() 即"确定"
        mutable std::vector<ftxui::Box> boxes; // 各按钮屏幕区域，供鼠标命中检测
    } popup_;

    ftxui::Element renderPopupBox() const;

protected:
    // 提示分级：错误 -> 弹窗；重要成功 -> 弹窗(notifyImportant)；次要成功 -> 底部消息行
    void notify(const core::Result& res) { setStatus(res.message, !res.success); }

    void notifyImportant(const core::Result& res) {
        if (res.success) showPopup("操作成功", res.message, false);
        else setStatus(res.message, true);
    }

    void setStatus(std::string msg, bool err = false) {
        if (err) {
            showPopup("操作失败", std::move(msg), true);
            return;
        }
        status_msg = std::move(msg);
        is_error = false;
    }

    // 打开模态弹窗 (错误时按消息关键词附带推荐导航)
    void showPopup(std::string title, std::string msg, bool err);

    // 构建消费者端顶部导航按钮组 (active_tab: 1=商城, 2=购物车, 3=领券, 4=个人中心, 0=其他如收银台)
    ftxui::Component buildCustomerNavBar(int active_tab);

    // 消费者端全局快捷键处理 (数字键 1-4 压栈切页，T 切换工作日/周末)
    bool handleCustomerShortcuts(const ftxui::Event& event, int active_tab);

    // 渲染消费者端公共顶栏 (资产摘要 + 导航按钮栏)
    ftxui::Element renderCustomerHeader(const ftxui::Component& nav_bar) const;

    // 渲染底部两行栏：消息行 + 快捷键提示行
    ftxui::Element renderFooterBar(const std::string& key_hints) const;

    // 组装消费者标准全屏页面框架 (顶栏 + 页面主体 + 底栏)
    ftxui::Element renderCustomerFrame(const ftxui::Component& nav_bar,
                                       ftxui::Element body,
                                       const std::string& key_hints) const;
};

} // namespace ui
