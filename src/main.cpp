#include "ui/Application.hpp"
#include <ftxui/component/app.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::filesystem::path defaultDataPath(const char* executable) {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size()) {
        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path() / "data";
    }
#elif defined(__linux__)
    std::error_code error;
    const auto binary = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) return binary.parent_path() / "data";
#endif
    return std::filesystem::absolute(executable).parent_path() / "data";
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path data_dir = defaultDataPath(argv[0]);
        bool mouse = true;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                std::cout << "VIP Coupon System — C++17 / FTXUI\n"
                          << "Usage: vip-coupon-system [--data-dir PATH] [--no-mouse]\n"
                          << "Default data: <executable directory>/data\n"
                          << "F2: mall, F3: cart, F4: coupons, F5: account, F6: simulated day\n"
                          << "Tab/Shift+Tab: focus, arrows: select, Enter: confirm, Esc: back, Ctrl+Q: quit\n";
                return 0;
            }
            if (arg == "--no-mouse") mouse = false;
            else if (arg == "--data-dir" && i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
                data_dir = std::filesystem::absolute(argv[++i]);
            else { std::cerr << "Unknown or incomplete option: " << arg << " (see --help)\n"; return 2; }
        }
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
        ui::AppContext context(data_dir);
        const auto result = context.initialize();
        if (!result.success) { std::cerr << result.message << '\n'; return 1; }
        auto screen = ftxui::App::Fullscreen();
        screen.TrackMouse(mouse);
        ui::Application application(context, screen.ExitLoopClosure());
        screen.Loop(application.component());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << '\n';
        return 1;
    }
}
