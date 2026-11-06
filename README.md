# 用户权益与优惠规划系统

C++17 终端应用：消费者可登录、浏览商品、管理购物车、领券并结算；管理员可管理商品、用户和优惠券。界面使用 FTXUI，业务数据保存在本地 JSON 文件中。

## 构建与运行

需要支持 C++17 的编译器、Git、CMake 3.20 或更高版本。首次配置时，CMake 会通过网络拉取 nlohmann/json 3.12.0 与 FTXUI 7.0.3。

Windows PowerShell（使用 Ninja）：

```powershell
git clone <仓库地址>
cd vip-coupon-system
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
.\bin\vip-coupon-system.exe
```

Linux / macOS：

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./bin/vip-coupon-system
```

也可使用 `cmake --build build --target run` 运行。Windows 请使用支持 UTF-8 的终端，例如 Windows Terminal。终端太窄时页面会竖向排列，可以通过 Tab 移动到页面下方控件。

## 演示账号

| 账号 | 密码 | 功能 |
|---|---|---|
| demo | 123456 | 消费者，初始模拟余额 2000 元 |
| staff | staff123 | 运营管理员：查看用户、补货、上架商品、发布优惠券 |
| root | admin123 | 超级管理员：运营功能，以及调整用户等级和重置系统 |

登录页也可以注册新消费者；沿用现有用户模块，新用户初始模拟余额为 1000 元。

## 操作

- `Tab` / `Shift+Tab` 在当前页面控件和上方操作区之间切换；`↑` / `↓` 选择列表项；`←` / `→` 切换选项；`Enter` 确认。也支持鼠标。
- 消费者使用 `F2` 商城、`F3` 购物车、`F4` 领券中心、`F5` 个人中心。`F6` 切换模拟工作日和周末。
- `Esc` 返回上一级；在商城或管理后台返回登录页；在登录页退出。`Ctrl+Q` 退出程序。
- 商城支持分类、搜索和加购；购物车支持改数量、移除、清空；个人中心支持查看等级进度、卡券、账单和模拟充值。
- 结算默认推荐最省钱的单张券，也可手动选券或不用券。页面展示商品原价、会员优惠、券优惠和最终实付；不可用券显示原因。确认时重新检查券、账单、余额和库存，支付后更新余额、券状态、会员等级、流水和库存。
- 管理后台分为用户、商品、发券和系统四个选项卡。切换到发券后填写券类型、费用、门槛、折扣或减免金额、适用品类、等级与时间；折扣券填 `0.85` 表示 85 折。

## 数据与边界

数据默认存放在可执行文件旁的 `data/` 目录，即通常的 `bin/data/`。首次运行会创建演示数据。也可以指定独立目录：

```sh
./bin/vip-coupon-system --data-dir /path/to/data
```

Windows 同样可以传入 `--data-dir`。`--no-mouse` 禁用鼠标追踪；`--help` 显示命令行帮助。

用户、商品和券池分别保存在 `users.json`、`products.json`、`coupons.json`。业务变更后会自动保存；购物车仅在当前登录期间保留。保存失败时界面会提示并允许重试；若三个数据文件不完整或无法加载，启动会停止，不会自动覆盖原文件。建议备份整个数据目录。超级管理员重置系统会替换所有演示数据，需要在界面输入 `RESET` 确认。

本项目为课程演示：充值和支付均为模拟操作，现有账户模块以明文保存密码，JSON 也不提供数据库级跨文件事务。请勿使用真实密码、真实支付信息或用于生产环境。

## 代码位置

- `include/ui/AppContext.hpp` / `src/ui/AppContext.cpp`：业务操作编排、输入检查、保存及支付校验。
- `include/ui/Application.hpp` / `src/ui/Application.cpp`：终端界面、页面与键盘交互。
- `src/main.cpp`：数据初始化、命令行参数和界面启动。
- `tests/test_tui_integration.cpp`：界面键盘导航与业务闭环测试。
