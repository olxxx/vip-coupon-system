#include "shop/ProductManager.hpp"
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "[Test Suite] shop::ProductManager 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 测试默认构造与初始空状态
    {
        shop::ProductManager mgr;
        assert(mgr.empty());
        assert(mgr.size() == 0);
        assert(!mgr.hasProduct("NON_EXIST"));
        assert(mgr.findProduct("NON_EXIST") == nullptr);
        assert(mgr.getProducts().empty());
        assert(mgr.getProductMap().empty());
        std::cout << "[PASS] 1. 默认构造与初始空状态校验" << std::endl;
    }

    // 2. 测试商品上架与防御拦截 (addProduct)
    {
        shop::ProductManager mgr;
        auto p1 = std::make_shared<shop::Product>("PROD_1001", "机械键盘", core::ProductCategory::DIGITAL, 299.0, 10);
        
        // 正常添加指针
        assert(mgr.addProduct(p1));
        assert(mgr.size() == 1);
        assert(mgr.hasProduct("PROD_1001"));

        // 拦截空指针
        assert(!mgr.addProduct(nullptr));
        assert(mgr.size() == 1);

        // 拦截重复 ID (防覆盖)
        auto p1_dup = std::make_shared<shop::Product>("PROD_1001", "重复键盘", core::ProductCategory::DIGITAL, 99.0, 5);
        assert(!mgr.addProduct(p1_dup));
        assert(mgr.size() == 1);
        assert(mgr.findProduct("PROD_1001")->getName() == "机械键盘"); // 属性未被篡改

        // 测试全参重载版本快捷上架
        assert(mgr.addProduct("PROD_1002", "无线鼠标", core::ProductCategory::DIGITAL, 99.0, 20));
        assert(mgr.size() == 2);
        assert(mgr.hasProduct("PROD_1002"));

        std::cout << "[PASS] 2. 商品上架与防御拦截逻辑校验" << std::endl;
    }

    // 3. 测试商品下架与清空 (removeProduct & clear)
    {
        shop::ProductManager mgr;
        mgr.addProduct("PROD_1001", "商品1", core::ProductCategory::FOOD, 10.0, 5);
        mgr.addProduct("PROD_1002", "商品2", core::ProductCategory::FOOD, 20.0, 5);

        // 下架不存在的商品
        assert(!mgr.removeProduct("PROD_404"));
        assert(mgr.size() == 2);

        // 成功下架
        assert(mgr.removeProduct("PROD_1001"));
        assert(mgr.size() == 1);
        assert(!mgr.hasProduct("PROD_1001"));
        assert(mgr.hasProduct("PROD_1002"));

        // 清空
        mgr.clear();
        assert(mgr.empty());
        assert(mgr.size() == 0);
        std::cout << "[PASS] 3. 商品下架与清空逻辑校验" << std::endl;
    }

    // 4. 测试库存操作事务性 (replenishProduct, deductProduct, setStock)
    {
        shop::ProductManager mgr;
        mgr.addProduct("PROD_1001", "库存测试商品", core::ProductCategory::DAILY, 50.0, 10);

        // 不存在的商品
        assert(!mgr.replenishProduct("NON_EXIST", 5));
        assert(!mgr.deductProduct("NON_EXIST", 5));
        assert(!mgr.setStock("NON_EXIST", 50));

        // 正常补货与非法补货拦截
        assert(mgr.replenishProduct("PROD_1001", 5));
        assert(mgr.findProduct("PROD_1001")->getStock() == 15);
        assert(!mgr.replenishProduct("PROD_1001", 0));
        assert(!mgr.replenishProduct("PROD_1001", -5));
        assert(mgr.findProduct("PROD_1001")->getStock() == 15);

        // 正常扣减与超额扣减拦截
        assert(mgr.deductProduct("PROD_1001", 5));
        assert(mgr.findProduct("PROD_1001")->getStock() == 10);
        assert(!mgr.deductProduct("PROD_1001", 100)); // 超额
        assert(mgr.findProduct("PROD_1001")->getStock() == 10);

        // 强制校准与非法负数校准拦截
        assert(mgr.setStock("PROD_1001", 80));
        assert(mgr.findProduct("PROD_1001")->getStock() == 80);
        assert(!mgr.setStock("PROD_1001", -1));
        assert(mgr.findProduct("PROD_1001")->getStock() == 80);

        std::cout << "[PASS] 4. 库存操作事务性与边界校验" << std::endl;
    }

    // 5. 测试价格修改与安全边界 (updatePrice)
    {
        shop::ProductManager mgr;
        mgr.addProduct("PROD_1001", "调价商品", core::ProductCategory::CLOTHING, 100.0, 10);

        assert(!mgr.updatePrice("NON_EXIST", 120.0));
        assert(mgr.updatePrice("PROD_1001", 129.9));
        assert(mgr.findProduct("PROD_1001")->getPrice() == 129.9);

        // 拦截 0 元与负价格
        assert(!mgr.updatePrice("PROD_1001", 0.0));
        assert(!mgr.updatePrice("PROD_1001", -10.0));
        assert(mgr.findProduct("PROD_1001")->getPrice() == 129.9);

        std::cout << "[PASS] 5. 调价与安全边界防呆校验" << std::endl;
    }

    // 6. 测试精确查找与模糊搜索 (findProduct & searchByName)
    {
        shop::ProductManager mgr;
        mgr.addProduct("PROD_1001", "Cherry 机械键盘", core::ProductCategory::DIGITAL, 499.0, 10);
        mgr.addProduct("PROD_1002", "蓝牙静音键盘", core::ProductCategory::DIGITAL, 199.0, 15);
        mgr.addProduct("PROD_1003", "人体工学鼠标", core::ProductCategory::DIGITAL, 99.0, 20);

        // 模糊搜索
        auto keyboards = mgr.searchByName("键盘");
        assert(keyboards.size() == 2);
        assert(keyboards[0]->getId() == "PROD_1001");
        assert(keyboards[1]->getId() == "PROD_1002");

        auto mice = mgr.searchByName("鼠标");
        assert(mice.size() == 1);
        assert(mice[0]->getId() == "PROD_1003");

        auto none = mgr.searchByName("显示器");
        assert(none.empty());

        std::cout << "[PASS] 6. 精确查找与名称模糊搜索校验" << std::endl;
    }

    // 7. 测试品类筛选与导出 (getProductsByCategory & searchByCategory)
    {
        shop::ProductManager mgr;
        mgr.addProduct("PROD_D1", "手机", core::ProductCategory::DIGITAL, 3999.0, 5);
        mgr.addProduct("PROD_D2", "平板", core::ProductCategory::DIGITAL, 2999.0, 8);
        mgr.addProduct("PROD_F1", "面包", core::ProductCategory::FOOD, 10.0, 50);

        auto digitals = mgr.getProductsByCategory(core::ProductCategory::DIGITAL);
        assert(digitals.size() == 2);

        auto foods = mgr.getProductsByCategory(core::ProductCategory::FOOD);
        assert(foods.size() == 1);
        assert(foods[0]->getId() == "PROD_F1");

        auto clothings = mgr.getProductsByCategory(core::ProductCategory::CLOTHING);
        assert(clothings.empty());

        // 测试 ALL 全场品类
        auto all_prods = mgr.getProductsByCategory(core::ProductCategory::ALL);
        assert(all_prods.size() == 3);

        auto cat_ids = mgr.searchByCategory(core::ProductCategory::DIGITAL);
        assert(cat_ids.size() == 2);

        std::cout << "[PASS] 7. 品类筛选与 ID 检索校验" << std::endl;
    }

    // 8. 测试稳定视图排序防抖动验证 (getProducts)
    {
        shop::ProductManager mgr;
        // 故意乱序插入
        mgr.addProduct("PROD_1005", "商品5", core::ProductCategory::OTHER, 1.0, 1);
        mgr.addProduct("PROD_1001", "商品1", core::ProductCategory::OTHER, 1.0, 1);
        mgr.addProduct("PROD_1003", "商品3", core::ProductCategory::OTHER, 1.0, 1);

        auto list = mgr.getProducts();
        assert(list.size() == 3);
        // 断言严格按 ID 升序排序
        assert(list[0]->getId() == "PROD_1001");
        assert(list[1]->getId() == "PROD_1003");
        assert(list[2]->getId() == "PROD_1005");

        std::cout << "[PASS] 8. 稳定排序视图防抖动校验" << std::endl;
    }

    // 9. 测试 JSON 序列化与全量往返恢复
    {
        shop::ProductManager mgr1;
        mgr1.addProduct("PROD_1001", "键盘", core::ProductCategory::DIGITAL, 299.0, 10);
        mgr1.addProduct("PROD_1002", "苹果", core::ProductCategory::FOOD, 5.5, 100);

        nlohmann::json j = mgr1.toJson();

        // 验证静态工厂与实例加载两种方式
        shop::ProductManager mgr2 = shop::ProductManager::fromJson(j);
        assert(mgr2.size() == 2);
        assert(mgr2.findProduct("PROD_1001")->getName() == "键盘");
        assert(mgr2.findProduct("PROD_1002")->getStock() == 100);

        shop::ProductManager mgr3;
        assert(mgr3.loadFromJson(j));
        assert(mgr3.size() == 2);
        assert(mgr3.hasProduct("PROD_1001"));
        assert(mgr3.hasProduct("PROD_1002"));

        std::cout << "[PASS] 9. JSON 序列化与反序列化全量往返校验" << std::endl;
    }

    // 10. 测试 JSON 格式破损防御与脏数据自动过滤
    {
        shop::ProductManager mgr;

        // 非 Object 顶层结构 (传入数组或标量)
        nlohmann::json invalid_array = nlohmann::json::array({1, 2, 3});
        assert(!mgr.loadFromJson(invalid_array));

        nlohmann::json invalid_string = "not_an_object";
        assert(!mgr.loadFromJson(invalid_string));

        // 包含脏数据的对象：商品1正常，商品2缺少价格，商品3缺少名称
        nlohmann::json mixed_json = {
            {"PROD_OK", {{"id", "PROD_OK"}, {"name", "健康商品"}, {"price", 88.0}}},
            {"PROD_BAD1", {{"id", "PROD_BAD1"}, {"name", "缺价格商品"}}},
            {"PROD_BAD2", {{"id", "PROD_BAD2"}, {"price", 99.0}}}
        };

        assert(mgr.loadFromJson(mixed_json));
        assert(mgr.size() == 1);
        assert(mgr.hasProduct("PROD_OK"));
        assert(!mgr.hasProduct("PROD_BAD1"));
        assert(!mgr.hasProduct("PROD_BAD2"));

        std::cout << "[PASS] 10. JSON 顶层结构防呆与坏数据自动过滤校验" << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "All ProductManager tests PASSED successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}