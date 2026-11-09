#include "ui/AppContext.hpp"
#include "ui/PageManager.hpp"
#include "ui/pages/LoginPage.hpp"
#include "ui/pages/MallPage.hpp"
#include "ui/pages/CartPage.hpp"
#include "ui/pages/CheckoutPage.hpp"
#include "ui/pages/CouponSquarePage.hpp"
#include "ui/pages/UserCenterPage.hpp"
#include "ui/pages/AdminPage.hpp"

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

// 辅助函数：将当前页面在 120x36 离屏画布中渲染为纯文本，验证无崩溃且关键文案存在
std::string render_to_text(ui::PageManager& mgr) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(36));
    ftxui::Render(screen, mgr.Render());
    return screen.ToString();
}

// 辅助函数：模拟鼠标在整个画布上无按键移动（仅悬停，不点击）
void sweep_mouse_hover(ui::PageManager& mgr) {
    for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 120; ++x) {
            ftxui::Mouse m;
            m.button = ftxui::Mouse::None;
            m.motion = ftxui::Mouse::Moved;
            m.x = x;
            m.y = y;
            mgr.OnEvent(ftxui::Event::Mouse("", m));
        }
    }
}
} // namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] FTXUI 终端交互与页面栈集成测试" << std::endl;
    std::cout << "========================================" << std::endl;

    namespace fs = std::filesystem;
    const fs::path test_dir = "build/test_ui_data";
    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    user::UserManager user_mgr;
    shop::ProductManager prod_mgr;
    shop::CouponManager cpn_mgr;
    shop::Cart cart;
    persist::StorageManager storage_mgr(test_dir);

    storage_mgr.initializeDataDirIfEmpty(user_mgr, prod_mgr, cpn_mgr);
    ui::AppContext ctx(user_mgr, prod_mgr, cpn_mgr, cart, storage_mgr);

    // 1. 验证增强种子数据 (demo 初始流水 + alice V2 白银会员)
    {
        auto demo = user_mgr.findCustomer("demo");
        assert(demo != nullptr);
        assert(!demo->getTransactions().empty());

        auto alice = user_mgr.findCustomer("alice");
        assert(alice != nullptr);
        assert(alice->getTier() == core::UserTier::V2_SILVER);
        assert(alice->getBalance() == 1200.0);
        assert(alice->getTotalSpent() == 950.0);
        assert(alice->hasCoupon("INST_1000"));
        assert(alice->getTransactions().size() >= 2);
        std::cout << "[PASS] 1. 种子数据验证通过 (demo 初始流水 & alice V2 白银会员)" << std::endl;
    }

    bool exited = false;
    auto mgr = ftxui::Make<ui::PageManager>(ctx, [&exited]() { exited = true; });

    // 2. 验证 PageManager 固定深度页面栈 (MAX_STACK_DEPTH = 8) 与生命周期
    {
        mgr->reset_stack(std::make_unique<ui::LoginPage>(ctx));
        assert(mgr->get_stack_depth() == 1);
        assert(mgr->get_current_page_name() == "系统登录");

        auto login_text = render_to_text(*mgr);
        assert(login_text.find("用户权益管理与优惠规划系统") != std::string::npos);
        assert(login_text.find("alice (白银会员)") != std::string::npos);

        // 先登录 demo 测试页面栈操作
        assert(ctx.loginCustomer("demo", "123456").success);
        mgr->reset_stack(std::make_unique<ui::MallPage>(ctx));
        assert(mgr->get_stack_depth() == 1);
        assert(mgr->get_current_page_name() == "商城货架");

        // 同名页面重复压栈应被忽略
        mgr->push_page(std::make_unique<ui::MallPage>(ctx));
        assert(mgr->get_stack_depth() == 1);

        // 连续交叉压栈超过 MAX_STACK_DEPTH (8)，验证栈深严格受限于 8
        for (int i = 0; i < 12; ++i) {
            if (i % 2 == 0) {
                mgr->push_page(std::make_unique<ui::CartPage>(ctx));
            } else {
                mgr->push_page(std::make_unique<ui::CouponSquarePage>(ctx));
            }
        }
        assert(mgr->get_stack_depth() == ui::PageManager::MAX_STACK_DEPTH);
        std::cout << "[PASS] 2. PageManager 固定深度栈 (MAX_STACK_DEPTH=8) 淘汰与防重机制验证通过" << std::endl;
    }

    // 3. 验证消费者完整购物、领券、凑单试算、支付与个人中心交互链路
    {
        mgr->reset_stack(std::make_unique<ui::MallPage>(ctx));
        auto mall_text = render_to_text(*mgr);
        assert(mall_text.find("demo") != std::string::npos);
        assert(mall_text.find("机械键盘") != std::string::npos);

        // 在商城货架按 '+' 增加数量；Enter 仅选中商品，需 → 进入详情区后在「加入购物车」按钮上按 Enter 才加购 (P1001 无线机械键盘 299.0 * 2 = 598.0)
        mgr->OnEvent(ftxui::Event::Character('+'));
        mgr->OnEvent(ftxui::Event::Return);
        assert(ctx.cart.getItemQuantity("P1001") == 0); // Enter 只选中，不加购
        mgr->OnEvent(ftxui::Event::ArrowRight);
        mgr->OnEvent(ftxui::Event::ArrowDown);
        mgr->OnEvent(ftxui::Event::Return);
        assert(ctx.cart.getItemQuantity("P1001") == 2);

        // 按快捷键 '3' 推入领券中心，按 'A' 一键领取全部免费券
        mgr->OnEvent(ftxui::Event::Character('3'));
        assert(mgr->get_current_page_name() == "领券中心");
        auto square_text = render_to_text(*mgr);
        assert(square_text.find("一键领取全部免费券") != std::string::npos);
        mgr->OnEvent(ftxui::Event::Character('A'));
        assert(ctx.user_mgr.getCurrentCustomer()->getCouponCount() >= 1);
        // 领取成功属于重要提示：弹出模态弹窗并独占按键，按 '2' 不应切页，Esc 关闭后才恢复
        mgr->OnEvent(ftxui::Event::Character('2'));
        assert(mgr->get_current_page_name() == "领券中心");
        mgr->OnEvent(ftxui::Event::Escape);
        assert(mgr->get_current_page_name() == "领券中心");

        // 按快捷键 '2' 推入购物车页，验证渲染与分品类统计
        mgr->OnEvent(ftxui::Event::Character('2'));
        assert(mgr->get_current_page_name() == "购物车");
        auto cart_text = render_to_text(*mgr);
        assert(cart_text.find("机械键盘") != std::string::npos);
        assert(cart_text.find("分品类金额统计") != std::string::npos);

        // 在购物车列表按 → 进入右侧操作区，↓↓ 到「前往收银台」按钮并按 Enter
        mgr->OnEvent(ftxui::Event::ArrowRight);
        mgr->OnEvent(ftxui::Event::ArrowDown);
        mgr->OnEvent(ftxui::Event::ArrowDown);
        mgr->OnEvent(ftxui::Event::Return);
        assert(mgr->get_current_page_name() == "收银台");
        auto checkout_text = render_to_text(*mgr);
        assert(checkout_text.find("最优推荐") != std::string::npos);
        assert(checkout_text.find("四维诊断") != std::string::npos);

        // 按 'T' 切换周末模拟环境
        bool prev_weekend = ctx.isWeekend();
        mgr->OnEvent(ftxui::Event::Character('T'));
        assert(ctx.isWeekend() == !prev_weekend);

        // 在收银台列表按 Enter 选中最优方案，→ 进入支付按钮并按 Enter 确认支付
        double before_balance = ctx.user_mgr.getCurrentCustomer()->getBalance();
        mgr->OnEvent(ftxui::Event::Return);
        assert(!ctx.cart.empty()); // 选中方案不会直接支付
        mgr->OnEvent(ftxui::Event::ArrowRight);
        mgr->OnEvent(ftxui::Event::Return);
        assert(ctx.cart.empty());
        assert(ctx.user_mgr.getCurrentCustomer()->getBalance() < before_balance);
        assert(ctx.user_mgr.getCurrentCustomer()->getTotalSpent() >= 500.0);
        // 消费满 500 应触发 V1 青铜会员自动升级
        assert(ctx.user_mgr.getCurrentCustomer()->getTier() >= core::UserTier::V1_BRONZE);

        auto receipt_text = render_to_text(*mgr);
        assert(receipt_text.find("订单支付已完成") != std::string::npos);

        // 按快捷键 '4' 进入个人中心验证多 Tab 切换、卡券管理、流水与密码修改
        mgr->OnEvent(ftxui::Event::Character('4'));
        assert(mgr->get_current_page_name() == "个人中心");
        auto uc_text = render_to_text(*mgr);
        assert(uc_text.find("个人中心") != std::string::npos);
        assert(uc_text.find("账户余额充值") != std::string::npos);

        // 切换到子 Tab 1 (我的卡券背包) 与 子 Tab 2 (账单流水历史)
        mgr->OnEvent(ftxui::Event::F2);
        auto uc_bag_text = render_to_text(*mgr);
        assert(uc_bag_text.find("我的卡券背包列表") != std::string::npos);
        assert(uc_bag_text.find("删除选中卡券") != std::string::npos);

        mgr->OnEvent(ftxui::Event::F3);
        auto uc_tx_text = render_to_text(*mgr);
        assert(uc_tx_text.find("多态交易流水列表") != std::string::npos);
        assert(uc_tx_text.find("交易流水凭证详情") != std::string::npos);

        double bal_before_recharge = ctx.user_mgr.getCurrentCustomer()->getBalance();
        assert(ctx.recharge(100.0).success);
        assert(ctx.user_mgr.getCurrentCustomer()->getBalance() == bal_before_recharge + 100.0);
        assert(ctx.cleanUsedCoupons().success);

        std::cout << "[PASS] 3. 消费者加购 -> 领券 -> 智能收银台 -> 自动晋级 -> 个人中心多 Tab 全链路通过" << std::endl;
    }

    // 4. 验证管理后台 3 大子页签渲染与 RBAC 权限拦截
    {
        ctx.logout();
        assert(ctx.loginAdmin("staff", "staff123").success);
        mgr->reset_stack(std::make_unique<ui::AdminPage>(ctx));
        assert(mgr->get_current_page_name() == "后台运营控制台");

        for (char tab_key : {'1', '2', '3'}) {
            mgr->OnEvent(ftxui::Event::Character(tab_key));
            auto admin_text = render_to_text(*mgr);
            assert(admin_text.find("后台运营控制台") != std::string::npos);
        }

        // 在用户等级管理页签校验 staff 只读权限展示与 RBAC 拦截
        auto user_tab_text = render_to_text(*mgr);
        assert(user_tab_text.find("仅只读查看") != std::string::npos);
        assert(!ctx.updateCustomerTier("demo", core::UserTier::V5_SUPREME).success);
        assert(!ctx.setCustomerBalance("demo", 5000.0).success);
        assert(!ctx.registerAdmin("newstaff", "123456", core::AdminRole::STAFF).success);
        assert(!ctx.resetSystem().success);

        // 切换为 root 超级管理员验证完整权限与新增后台接口
        ctx.logout();
        assert(ctx.loginAdmin("root", "admin123").success);
        mgr->reset_stack(std::make_unique<ui::AdminPage>(ctx));
        mgr->OnEvent(ftxui::Event::Character('3'));
        auto root_tab_text = render_to_text(*mgr);
        assert(root_tab_text.find("允许调级/删号") != std::string::npos);
        assert(ctx.updateCustomerTier("demo", core::UserTier::V3_GOLD).success);
        assert(ctx.setCustomerBalance("demo", 3000.0).success);
        assert(user_mgr.findCustomer("demo")->getBalance() == 3000.0);

        // 验证商品扣库存、改名、改分类与管理员账号管理
        assert(ctx.deductProduct("P1001", 1).success);
        assert(ctx.updateProductName("P1001", "无线RGB机械键盘").success);
        assert(prod_mgr.findProduct("P1001")->getName() == "无线RGB机械键盘");
        assert(ctx.updateProductCategory("P1001", core::ProductCategory::DIGITAL).success);
        assert(ctx.registerAdmin("ops2", "ops123", core::AdminRole::STAFF).success);
        assert(ctx.updateAdminRole("ops2", core::AdminRole::SUPER_ADMIN).success);
        assert(ctx.removeAdmin("ops2").success);

        // 在后台栈底按 Esc 应登出并回到系统登录页，再按 Esc 触发退出回调
        mgr->OnEvent(ftxui::Event::Escape);
        assert(mgr->get_current_page_name() == "系统登录");
        mgr->OnEvent(ftxui::Event::Escape); // 登录页输入框聚焦：首次 Esc 仅退出输入
        assert(!exited);
        mgr->OnEvent(ftxui::Event::Escape);
        assert(exited);

        std::cout << "[PASS] 4. 管理后台 3 大模块渲染、RBAC 权限分级与新增管理接口验证通过" << std::endl;
    }

    // 5. 验证鼠标悬停不会抢走输入框焦点，以及卡券删除、设数量、改密码功能
    {
        ctx.logout();
        mgr->reset_stack(std::make_unique<ui::LoginPage>(ctx));
        render_to_text(*mgr); // 布局一次，记录各控件区域
        mgr->OnEvent(ftxui::Event::Character('x'));
        mgr->OnEvent(ftxui::Event::Character('y'));
        sweep_mouse_hover(*mgr);
        mgr->OnEvent(ftxui::Event::Character('z'));
        assert(render_to_text(*mgr).find("xyz") != std::string::npos);

        assert(ctx.loginCustomer("alice", "123456").success);
        assert(ctx.user_mgr.getCurrentCustomer()->hasCoupon("INST_1000"));
        assert(ctx.removeUserCoupon("INST_1000").success);
        assert(!ctx.user_mgr.getCurrentCustomer()->hasCoupon("INST_1000"));

        assert(ctx.addToCart("P1001", 1).success);
        assert(ctx.setCartQty("P1001", 5).success);
        assert(ctx.cart.getItemQuantity("P1001") == 5);
        assert(ctx.setCartQty("P1001", 0).success);
        assert(!ctx.cart.hasItem("P1001"));

        assert(ctx.changePassword("123456", "654321").success);
        assert(ctx.user_mgr.getCurrentCustomer()->verifyPassword("654321"));
        assert(ctx.changePassword("654321", "123456").success);

        mgr->reset_stack(std::make_unique<ui::MallPage>(ctx));
        render_to_text(*mgr);
        mgr->OnEvent(ftxui::Event::Character('/'));
        mgr->OnEvent(ftxui::Event::Character('k'));
        sweep_mouse_hover(*mgr);
        mgr->OnEvent(ftxui::Event::Character('b'));
        assert(render_to_text(*mgr).find("kb") != std::string::npos);

        // 输入框聚焦时 Esc 只退出输入，不返回上一页
        mgr->push_page(std::make_unique<ui::CartPage>(ctx));
        mgr->pop_page();
        assert(mgr->get_current_page_name() == "商城货架");
        mgr->OnEvent(ftxui::Event::Character('/'));
        mgr->OnEvent(ftxui::Event::Escape);
        assert(mgr->get_current_page_name() == "商城货架");
        std::cout << "[PASS] 5. 鼠标悬停、Esc 退出输入、单张卡券删除、设数量与改密码验证通过" << std::endl;
    }

    if (fs::exists(test_dir)) {
        fs::remove_all(test_dir);
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有 FTXUI 终端交互集成测试 100% 通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
