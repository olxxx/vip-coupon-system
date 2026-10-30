#include "shop/Product.hpp"
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] shop::Product 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 测试默认构造函数与初始状态
    {
        shop::Product p_default;
        assert(p_default.getId().empty());
        assert(p_default.getName().empty());
        assert(p_default.getCategory() == core::ProductCategory::OTHER);
        assert(p_default.getPrice() == 0.0);
        assert(p_default.getStock() == 0);
        assert(p_default.isOutOfStock());
        assert(!p_default.hasStock());
        std::cout << "[PASS] 1. 默认构造函数与初始状态校验" << std::endl;
    }

    // 2. 测试全参构造函数与合法值
    {
        shop::Product p("PROD_1001", "机械键盘", core::ProductCategory::DIGITAL, 299.50, 10);
        assert(p.getId() == "PROD_1001");
        assert(p.getName() == "机械键盘");
        assert(p.getCategory() == core::ProductCategory::DIGITAL);
        assert(p.getPrice() == 299.50);
        assert(p.getStock() == 10);
        assert(p.hasStock());
        assert(p.hasStock(10));
        assert(!p.hasStock(11));
        assert(!p.isOutOfStock());
        std::cout << "[PASS] 2. 全参构造函数与属性访问器校验" << std::endl;
    }

    // 3. 测试构造函数负数值防御保护
    {
        shop::Product p_neg("PROD_NEG", "负数商品", core::ProductCategory::FOOD, -199.99, -50);
        assert(p_neg.getPrice() == 0.0); // 负价格自动规整为 0.0
        assert(p_neg.getStock() == 0);   // 负库存自动规整为 0
        assert(p_neg.isOutOfStock());
        std::cout << "[PASS] 3. 构造函数负数价格/库存防呆校验" << std::endl;
    }

    // 4. 测试库存操作 (扣减、补货、校准)
    {
        shop::Product p("PROD_1002", "测试库存", core::ProductCategory::DAILY, 50.0, 10);

        // 正常扣减
        assert(p.deductStock(3));
        assert(p.getStock() == 7);

        // 非法扣减与超额扣减 (不应影响原库存)
        assert(!p.deductStock(0));
        assert(!p.deductStock(-5));
        assert(!p.deductStock(8)); // 超过现有库存 7
        assert(p.getStock() == 7);

        // 正常补货与非法补货
        assert(p.replenishStock(5));
        assert(p.getStock() == 12);
        assert(!p.replenishStock(0));
        assert(!p.replenishStock(-10));
        assert(p.getStock() == 12);

        // 管理员强制校准库存
        assert(p.setStock(100));
        assert(p.getStock() == 100);
        assert(!p.setStock(-1)); // 拦截负库存
        assert(p.getStock() == 100);
        std::cout << "[PASS] 4. 库存扣减、补货与校准逻辑校验" << std::endl;
    }

    // 5. 测试属性修改器 (Setter)
    {
        shop::Product p("PROD_1003", "原名称", core::ProductCategory::CLOTHING, 100.0, 5);

        // 修改价格
        assert(p.setPrice(129.99));
        assert(p.getPrice() == 129.99);
        assert(!p.setPrice(-0.01)); // 拦截负价格
        assert(p.getPrice() == 129.99);

        // 修改名称
        assert(p.setName("新名称"));
        assert(p.getName() == "新名称");
        assert(!p.setName("")); // 拦截空名称
        assert(p.getName() == "新名称");

        // 修改品类
        p.setCategory(core::ProductCategory::OTHER);
        assert(p.getCategory() == core::ProductCategory::OTHER);
        assert(p.getCategoryName() == "其他");
        std::cout << "[PASS] 5. 属性 Setter 防御逻辑校验" << std::endl;
    }

    // 6. 测试格式化输出与比较运算符
    {
        shop::Product p1("PROD_1004", "冲锋衣", core::ProductCategory::CLOTHING, 888.8, 2);
        shop::Product p2("PROD_1004", "同ID不同名", core::ProductCategory::DIGITAL, 99.0, 10);
        shop::Product p3("PROD_1005", "冲锋衣", core::ProductCategory::CLOTHING, 888.8, 2);

        // 两位小数格式化
        assert(p1.getFormattedPrice() == "888.80");

        // operator== 基于 ID 判等
        assert(p1 == p2);
        assert(!(p1 == p3));

        // 摘要文本包含关键信息
        std::string summary = p1.getDisplaySummary();
        assert(summary.find("PROD_1004") != std::string::npos);
        assert(summary.find("冲锋衣") != std::string::npos);
        assert(summary.find("服饰鞋包") != std::string::npos);
        std::cout << "[PASS] 6. 格式化输出与 operator== 判等校验" << std::endl;
    }

    // 7. 测试 JSON 序列化与反序列化
    {
        shop::Product original("PROD_1006", "降噪耳机", core::ProductCategory::DIGITAL, 599.00, 15);
        nlohmann::json j = original.toJson();

        assert(j["id"] == "PROD_1006");
        assert(j["name"] == "降噪耳机");
        assert(j["category"] == static_cast<int>(core::ProductCategory::DIGITAL));
        assert(j["price"] == 599.00);
        assert(j["stock"] == 15);

        shop::Product restored = *shop::Product::fromJson(j);
        assert(restored == original);
        assert(restored.getName() == original.getName());
        assert(restored.getCategory() == original.getCategory());
        assert(restored.getPrice() == original.getPrice());
        assert(restored.getStock() == original.getStock());
        std::cout << "[PASS] 7. JSON 序列化与反序列化往返校验" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "All Product tests PASSED successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
