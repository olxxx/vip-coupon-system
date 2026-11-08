#include "ui/PageManager.hpp"
#include "ui/pages/LoginPage.hpp"

namespace ui {

using namespace ftxui;

PageManager::PageManager(AppContext& context, ScreenInteractive& screen)
    : ctx(context), exit_closure(screen.ExitLoopClosure()) {}

PageManager::PageManager(AppContext& context, std::function<void()> on_exit)
    : ctx(context), exit_closure(std::move(on_exit)) {}

void PageManager::sync_context_stack_info() {
    ctx.stack_depth = page_stack.size();
    ctx.prev_page_name = get_previous_page_name();
}

void PageManager::push_page(std::unique_ptr<Page> page) {
    if (!page) return;
    if (!page_stack.empty() && page_stack.back()->get_page_name() == page->get_page_name()) {
        return;
    }
    if (page_stack.size() >= MAX_STACK_DEPTH) {
        page_stack.front()->on_exit();
        page_stack.erase(page_stack.begin());
    }
    push_internal(std::move(page));
}

void PageManager::pop_page() {
    if (page_stack.size() > 1) {
        page_stack.back()->on_exit();
        page_stack.pop_back();
        page_stack.back()->on_enter();
        sync_context_stack_info();
    } else if (!page_stack.empty() && page_stack.back()->get_page_name() != "系统登录") {
        // 栈底且不在登录页（如在商城首页或后台首页按 Esc）：登出并重置回登录页
        ctx.logout();
        reset_stack(std::make_unique<LoginPage>(ctx));
    } else {
        // 已在登录页栈底按 Esc：退出全屏循环
        if (exit_closure) exit_closure();
    }
}

void PageManager::change_page(std::unique_ptr<Page> page) {
    if (!page) return;
    if (!page_stack.empty()) {
        page_stack.back()->on_exit();
        page_stack.pop_back();
    }
    push_internal(std::move(page));
}

void PageManager::reset_stack(std::unique_ptr<Page> root_page) {
    while (!page_stack.empty()) {
        page_stack.back()->on_exit();
        page_stack.pop_back();
    }
    if (root_page) {
        push_internal(std::move(root_page));
    } else {
        sync_context_stack_info();
    }
}

std::string PageManager::get_previous_page_name() const {
    if (page_stack.size() >= 2) {
        return page_stack[page_stack.size() - 2]->get_page_name();
    }
    return "";
}

std::string PageManager::get_current_page_name() const {
    if (!page_stack.empty()) {
        return page_stack.back()->get_page_name();
    }
    return "";
}

bool PageManager::OnEvent(Event event) {
    if (event == Event::CtrlQ) {
        if (exit_closure) exit_closure();
        return true;
    }
    if (page_stack.empty()) return false;

    PageAction action = PageAction::none();
    bool handled = page_stack.back()->handle_event(event, action);

    switch (action.type) {
        case PageAction::Type::None:   return handled;
        case PageAction::Type::Pop:    pop_page(); return true;
        case PageAction::Type::Push:   push_page(std::move(action.next_page)); return true;
        case PageAction::Type::Change: change_page(std::move(action.next_page)); return true;
        case PageAction::Type::Reset:  reset_stack(std::move(action.next_page)); return true;
        case PageAction::Type::Quit:   if (exit_closure) exit_closure(); return true;
    }
    return handled;
}

Element PageManager::OnRender() {
    if (page_stack.empty()) {
        return text("系统无活动页面") | center;
    }
    return page_stack.back()->render();
}

void PageManager::push_internal(std::unique_ptr<Page> page) {
    page->on_enter();
    page_stack.push_back(std::move(page));
    sync_context_stack_info();
}

} // namespace ui
