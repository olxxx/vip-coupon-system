#include "shop/Cart.hpp"
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] shop::Cart 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 准备测试商品
    auto p1 = std::make_shared<shop::Product>("P1001", "机械键盘", core::ProductCategory::DIGITAL, 300.0, 5);
    auto p2 = std::make_shared<shop::Product>("P1002", "运动外套", core::ProductCategory::CLOTHING, 200.0, 10);
    auto p3 = std::make_shared<shop::Product>("P1003", "有机苹果", core::ProductCategory::FOOD, 10.0, 50);

    // 1. 默认构造函数与初始空状态校验
    {
        shop::Cart cart;
        assert(cart.empty());
        assert(cart.isEmpty());
        assert(cart.getDistinctCount() == 0);
        assert(cart.getTotalQuantity() == 0);
        assert(cart.getTotalOriginalAmount() == 0.0);
        assert(cart.getCategoryAmount(core::ProductCategory::ALL) == 0.0);
        assert(cart.getCategoryAmount(core::ProductCategory::DIGITAL) == 0.0);
        assert(cart.getItemslist().empty());
        assert(cart.getItemsMap().empty());
        std::cout << "[PASS] 1. 默认构造与初始空状态校验" << std::endl;
    }

    // 2. 基础加购 addItem 与顺序稳定性校验
    {
        shop::Cart cart;
        assert(cart.addItem(p1, 2)); // 机械键盘 * 2
        assert(cart.addItem(p2, 3)); // 运动外套 * 3
        assert(cart.addItem(p3, 5)); // 有机苹果 * 5

        assert(!cart.empty());
        assert(cart.getDistinctCount() == 3);
        assert(cart.getTotalQuantity() == 2 + 3 + 5); // 10 件

        assert(cart.hasItem("P1001"));
        assert(cart.getItemQuantity("P1001") == 2);
        assert(cart.getItemQuantity("P1002") == 3);
        assert(cart.getItemQuantity("NOT_EXIST") == 0);

        // 验证按加购顺序返回的稳定列表
        auto list = cart.getItemslist();
        assert(list.size() == 3);
        assert(list[0].getProductId() == "P1001");
        assert(list[1].getProductId() == "P1002");
        assert(list[2].getProductId() == "P1003");
        std::cout << "[PASS] 2. 基础加购 addItem 与列表顺序稳定性校验" << std::endl;
    }

    // 3. 加购边界防呆与累计防超卖校验
    {
        shop::Cart cart;
        // 非法参数防御
        assert(!cart.addItem(nullptr, 1));
        assert(!cart.addItem(p1, 0));
        assert(!cart.addItem(p1, -3));

        // p1 总库存为 5
        assert(cart.addItem(p1, 3)); // 已加购 3，剩余库存还可加 2
        assert(cart.getItemQuantity("P1001") == 3);

        // 尝试再次加购 3 件 (3 + 3 = 6 > 5)，必须被严格拦截！
        assert(!cart.addItem(p1, 3));
        assert(cart.getItemQuantity("P1001") == 3); // 数量应保持 3

        // 再次加购 2 件 (3 + 2 = 5 <= 5)，允许加满
        assert(cart.addItem(p1, 2));
        assert(cart.getItemQuantity("P1001") == 5);

        // 此时已达上限，即使加购 1 件也应被拦截
        assert(!cart.addItem(p1, 1));
        std::cout << "[PASS] 3. 加购防御性与累计防超卖逻辑校验" << std::endl;
    }

    // 4. 金额计算与分品类核算 (含 ALL 全场通用) 校验
    {
        shop::Cart cart;
        // p1: 300.0 * 2 = 600.0 (DIGITAL)
        // p2: 200.0 * 1 = 200.0 (CLOTHING)
        // p3: 10.0 * 10 = 100.0 (FOOD)
        cart.addItem(p1, 2);
        cart.addItem(p2, 1);
        cart.addItem(p3, 10);

        double expected_total = 600.0 + 200.0 + 100.0; // 900.0
        assert(cart.getTotalOriginalAmount() == expected_total);

        // 各品类分别核算
        assert(cart.getCategoryAmount(core::ProductCategory::DIGITAL) == 600.0);
        assert(cart.getCategoryAmount(core::ProductCategory::CLOTHING) == 200.0);
        assert(cart.getCategoryAmount(core::ProductCategory::FOOD) == 100.0);
        assert(cart.getCategoryAmount(core::ProductCategory::DAILY) == 0.0); // 购物车无日用品

        // ALL 全场通用必须等于原价总额
        assert(cart.getCategoryAmount(core::ProductCategory::ALL) == expected_total);
        std::cout << "[PASS] 4. 原价总额与分品类金额核算校验" << std::endl;
    }

    // 5. 数量加减与修改 (increase / decrease / update) 校验
    {
        shop::Cart cart;
        cart.addItem(p1, 2); // p1 stock = 5

        // increaseQuantity 正常增加与上限防御
        assert(cart.increaseQuantity("P1001", 2)); // 变 4
        assert(cart.getItemQuantity("P1001") == 4);
        assert(!cart.increaseQuantity("P1001", 2)); // 4+2=6 > 5，拦截！
        assert(cart.getItemQuantity("P1001") == 4);
        assert(!cart.increaseQuantity("P1001", -1)); // 拦截负步进

        // decreaseQuantity 正常减少
        assert(cart.decreaseQuantity("P1001", 1)); // 变 3
        assert(cart.getItemQuantity("P1001") == 3);
        assert(!cart.decreaseQuantity("P1001", 5)); // 减超了，拦截！
        assert(cart.getItemQuantity("P1001") == 3);

        // 减至 0 自动从购物车移除
        assert(cart.decreaseQuantity("P1001", 3));
        assert(!cart.hasItem("P1001"));
        assert(cart.empty());

        // updateQuantity 修改与置 0 移除
        cart.addItem(p2, 5); // p2 stock = 10
        assert(cart.updateQuantity("P1002", 2));
        assert(cart.getItemQuantity("P1002") == 2);
        assert(!cart.updateQuantity("P1002", 99)); // 超过库存上限 10，拦截！
        assert(cart.getItemQuantity("P1002") == 2); // 数量保持不变
        assert(!cart.updateQuantity("P1002", -1)); // 拦截负数
        assert(cart.updateQuantity("P1002", 0));  // 置 0 自动移除
        assert(!cart.hasItem("P1002"));
        assert(cart.empty());
        std::cout << "[PASS] 5. 数量加减修改与归零自动移除校验" << std::endl;
    }

    // 6. 移除与清空 (removeItem & clear) 校验
    {
        shop::Cart cart;
        cart.addItem(p1, 1);
        cart.addItem(p2, 1);
        cart.addItem(p3, 1);

        assert(cart.removeItem("P1002"));
        assert(!cart.hasItem("P1002"));
        assert(cart.getDistinctCount() == 2);
        // 验证 item_order 同步移除了 P1002
        auto list = cart.getItemslist();
        assert(list.size() == 2);
        assert(list[0].getProductId() == "P1001");
        assert(list[1].getProductId() == "P1003");

        // 再次移除不存在商品
        assert(!cart.removeItem("P1002"));

        // 清空
        cart.clear();
        assert(cart.empty());
        assert(cart.getItemslist().empty());
        std::cout << "[PASS] 6. removeItem 与 clear 清空校验" << std::endl;
    }

    // 7. 库存前置可用性校验 validateStockAvailable
    {
        // 独立商品副本避免污染
        auto prodA = std::make_shared<shop::Product>("A", "商品A", core::ProductCategory::OTHER, 50.0, 3);
        shop::Cart cart;
        cart.addItem(prodA, 3);

        std::string err;
        assert(cart.validateStockAvailable(err)); // 充足

        // 模拟后台库存发生变动减少
        prodA->deductStock(1); // 真实库存变成了 2，而购物车需要 3
        assert(!cart.validateStockAvailable(err));
        assert(!err.empty());
        assert(err.find("商品A") != std::string::npos);
        std::cout << "[PASS] 7. validateStockAvailable 库存诊断校验" << std::endl;
    }

    // 8. 批量扣库存 deductAllStock 的两阶段事务原子性校验
    {
        auto item1 = std::make_shared<shop::Product>("T1", "测试商品1", core::ProductCategory::OTHER, 10.0, 5);
        auto item2 = std::make_shared<shop::Product>("T2", "测试商品2", core::ProductCategory::OTHER, 20.0, 2);

        shop::Cart cart;
        cart.addItem(item1, 3); // 需要 3 个 (库存 5)
        cart.addItem(item2, 2); // 需要 2 个 (库存 2)

        // 场景 A: item2 发生缺货 (外部扣减了 1 个，只剩 1 个)
        item2->deductStock(1); // item2 现在只有 1 个

        // 执行 deductAllStock，必须触发两阶段回滚，返回 false，且 item1 的库存绝不能被扣减！
        assert(!cart.deductAllStock());
        assert(item1->getStock() == 5); // item1 依然是 5，绝未被脏扣！
        assert(cart.getDistinctCount() == 2); // 购物车不会被提前清空

        // 场景 B: 补足库存后正常扣减
        item2->replenishStock(5); // item2 补到 6 个
        assert(cart.deductAllStock()); // 扣减成功！
        assert(item1->getStock() == 2); // 5 - 3 = 2
        assert(item2->getStock() == 4); // 6 - 2 = 4
        assert(cart.empty());           // 购物车自动清空
        std::cout << "[PASS] 8. deductAllStock 两阶段事务扣减原子性校验" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "All Cart tests PASSED successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
