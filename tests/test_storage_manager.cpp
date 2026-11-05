#include "persist/StorageManager.hpp"
#include "user/UserManager.hpp"
#include "shop/ProductManager.hpp"
#include "coupon/CouponManager.hpp"
#include "coupon/ConcreteCoupons.hpp"
#include "common/Types.hpp"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <memory>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] StorageManager 持久化仓储与冷启动测试" << std::endl;
    std::cout << "========================================" << std::endl;

    namespace fs = std::filesystem;
    const fs::path test_dir = "build/test_persist_data";

    // 0. 清理旧测试目录
    if (fs::exists(test_dir))
    {
        fs::remove_all(test_dir);
    }

    persist::StorageManager storage(test_dir);

    // 1. 初始环境检查 (未初始化)
    {
        assert(!storage.isDataInitialized());
        assert(storage.getDataDir() == test_dir);
        assert(storage.getUsersFilePath() == test_dir / "users.json");
        assert(storage.getProductsFilePath() == test_dir / "products.json");
        assert(storage.getCouponsFilePath() == test_dir / "coupons.json");
        std::cout << "[PASS] 1. 初始目录与未初始化状态检查通过" << std::endl;
    }

    // 2. 冷启动种子数据注入 (initializeDataDirIfEmpty)
    user::UserManager user_mgr;
    shop::ProductManager prod_mgr;
    shop::CouponManager cpn_mgr;

    {
        assert(user_mgr.empty());
        assert(prod_mgr.empty());
        assert(cpn_mgr.empty());

        // 首次运行，自动创建目录并注入种子数据
        storage.initializeDataDirIfEmpty(user_mgr, prod_mgr, cpn_mgr);

        assert(storage.isDataInitialized());
        assert(fs::exists(storage.getUsersFilePath()));
        assert(fs::exists(storage.getProductsFilePath()));
        assert(fs::exists(storage.getCouponsFilePath()));

        // 验证默认注入的管理账号
        assert(user_mgr.hasAdmin("root"));
        assert(user_mgr.hasAdmin("staff"));
        auto root_admin = user_mgr.findAdmin("root");
        assert(root_admin && root_admin->getRole() == core::AdminRole::SUPER_ADMIN);

        // 验证默认注入的测试消费者
        assert(user_mgr.hasCustomer("demo"));
        auto demo_user = user_mgr.findCustomer("demo");
        assert(demo_user && demo_user->getBalance() >= 2000.0);

        // 验证默认商品库
        assert(prod_mgr.size() >= 10);
        assert(prod_mgr.hasProduct("P1001")); // 机械键盘
        assert(prod_mgr.hasProduct("P5001")); // 限量版手办 (售罄项)
        assert(prod_mgr.findProduct("P5001")->isOutOfStock());

        // 验证默认优惠券库
        assert(cpn_mgr.size() >= 5);
        assert(cpn_mgr.hasCoupon("TPL_CASH_05"));
        assert(cpn_mgr.hasCoupon("TPL_THRESHOLD_200_30"));

        std::cout << "[PASS] 2. 冷启动种子数据注入与磁盘自动落盘测试通过" << std::endl;
    }

    // 3. 全量数据恢复 (loadAll)
    {
        user::UserManager restored_users;
        shop::ProductManager restored_prods;
        shop::CouponManager restored_cpns;

        assert(restored_users.empty());
        assert(restored_prods.empty());
        assert(restored_cpns.empty());

        assert(storage.loadAll(restored_users, restored_prods, restored_cpns));

        assert(restored_users.hasAdmin("root"));
        assert(restored_users.hasCustomer("demo"));
        assert(restored_prods.hasProduct("P1001"));
        assert(restored_cpns.hasCoupon("TPL_CASH_05"));
        assert(restored_cpns.getInstanceCounter() >= 1000);

        std::cout << "[PASS] 3. loadAll 全量反序列化恢复测试通过" << std::endl;
    }

    // 4. 单项模块读写 (saveUsers / saveProducts / saveCoupons)
    {
        // A. 修改用户数据并单独存盘
        user_mgr.registerCustomer("new_buyer", "123456");
        assert(storage.saveUsers(user_mgr));

        user::UserManager verify_users;
        assert(storage.loadUsers(verify_users));
        assert(verify_users.hasCustomer("new_buyer"));

        // B. 修改商品库并单独存盘
        prod_mgr.addProduct("P9999", "测试新商品", core::ProductCategory::OTHER, 99.9, 10);
        assert(storage.saveProducts(prod_mgr));

        shop::ProductManager verify_prods;
        assert(storage.loadProducts(verify_prods));
        assert(verify_prods.hasProduct("P9999"));

        // C. 修改卡券并单独存盘
        cpn_mgr.registerCouponToPool(std::make_shared<shop::CashCoupon>(
            "TPL_NEW_CASH", "单项存盘立减券", 0.0, 10.0,
            core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
            core::TimeConstraint::ANYTIME
        ));
        assert(storage.saveCoupons(cpn_mgr));

        shop::CouponManager verify_cpns;
        assert(storage.loadCoupons(verify_cpns));
        assert(verify_cpns.hasCoupon("TPL_NEW_CASH"));

        std::cout << "[PASS] 4. 单项模块即时落盘与读取测试通过" << std::endl;
    }

    // 5. 出厂设置重置功能 (resetToDefault)
    {
        assert(user_mgr.hasCustomer("new_buyer"));
        assert(prod_mgr.hasProduct("P9999"));

        assert(storage.resetToDefault(user_mgr, prod_mgr, cpn_mgr));

        // 重新变回出厂种子数据
        assert(!user_mgr.hasCustomer("new_buyer"));
        assert(!prod_mgr.hasProduct("P9999"));
        assert(user_mgr.hasCustomer("demo"));
        assert(prod_mgr.hasProduct("P1001"));

        // 磁盘文件同步更新
        user::UserManager check_users;
        storage.loadUsers(check_users);
        assert(!check_users.hasCustomer("new_buyer"));

        std::cout << "[PASS] 5. resetToDefault 重置出厂设置测试通过" << std::endl;
    }

    // 6. 清理临时测试目录
    if (fs::exists(test_dir))
    {
        fs::remove_all(test_dir);
    }

    std::cout << "========================================" << std::endl;
    std::cout << "所有 StorageManager 单元测试 100% 通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
