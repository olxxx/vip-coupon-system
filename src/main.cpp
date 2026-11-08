#ifdef _WIN32
#include <windows.h>
#endif

#include "ui/AppContext.hpp"
#include "ui/PageManager.hpp"
#include "ui/pages/LoginPage.hpp"

#include <ftxui/component/screen_interactive.hpp>
#include <memory>

int main() {
#ifdef _WIN32
    // 强制控制台输入输出编码为 UTF-8，确保中文与 Box-Drawing 字符正常渲染
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 1. 初始化核心业务管理器与持久化引擎
    user::UserManager user_mgr;
    shop::ProductManager prod_mgr;
    shop::CouponManager cpn_mgr;
    shop::Cart cart;
    persist::StorageManager storage_mgr("data");

    // 2. 冷启动检测：若 data/ 目录缺失则自动生成内置演示种子数据，否则从 JSON 加载
    storage_mgr.initializeDataDirIfEmpty(user_mgr, prod_mgr, cpn_mgr);

    // 3. 构造全局应用上下文
    ui::AppContext ctx(user_mgr, prod_mgr, cpn_mgr, cart, storage_mgr);

    // 4. 启动 FTXUI 全屏交互主循环与固定深度页面栈管理器
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto page_mgr = ftxui::Make<ui::PageManager>(ctx, screen);
    page_mgr->reset_stack(std::make_unique<ui::LoginPage>(ctx));

    screen.Loop(page_mgr);

    // 5. 退出前自动刷盘保存最新状态
    storage_mgr.saveAll(user_mgr, prod_mgr, cpn_mgr);
    return 0;
}
