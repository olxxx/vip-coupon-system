#ifdef _WIN32
#include <windows.h>

// 激活 Windows 终端 UTF-8 与虚拟终端转义序列 (VT Mode)
namespace {

void InitWindowsConsole() {
    // 1. 设置控制台输入输出代码页为 UTF-8，防止中文与制表符乱码
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 2. 显式激活 Windows 虚拟终端转义序列 (VT Mode)，防止 ANSI 颜色代码作为乱码字面量打印
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}

} // namespace
#endif

#include "ui/AppContext.hpp"
#include "ui/PageManager.hpp"
#include "ui/pages/LoginPage.hpp"

#include <ftxui/component/screen_interactive.hpp>
#include <memory>

int main() {
#ifdef _WIN32
    InitWindowsConsole();
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
