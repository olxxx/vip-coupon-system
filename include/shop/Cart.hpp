#pragma once
#include "shop/Product.hpp"
#include "common/Types.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace shop
{

// 购物车明细项封装
struct CartItem
{
    std::shared_ptr<Product> product;
    int quantity;

    CartItem() = default;
    CartItem(std::shared_ptr<Product> prod, int qua)
        : product(std::move(prod)), quantity(std::max(0, qua)) {}

    double getSubtotal() const
    {
        return (product && quantity > 0) ? product->getPrice() * quantity : 0.0;
    }

    bool isStockSufficient() const
    {
        return product && product->getStock() >= quantity;
    }

    std::string getProductId() const { return product ? product->getId() : ""; }
    std::string getProductName() const { return product ? product->getName() : ""; }
    double getProductPrice() const { return product ? product->getPrice() : 0.0; }
    int getProductStock() const { return product ? product->getStock() : 0; }
    core::ProductCategory getProductCategory() const { return product ? product->getCategory() : core::ProductCategory::OTHER; }
};

class Cart
{
private:
    std::unordered_map<std::string, CartItem> items; // 商品ID到明细项的哈希映射
    std::vector<std::string> item_order;            // 记录加购先后顺序，供UI稳定列表展示

public:
    Cart() = default;

    // ========================================================
    // 1. 状态判断与规模统计
    // ========================================================
    // 检查购物车是否为空
    bool empty() const { return items.empty(); }
    bool isEmpty() const { return items.empty(); }

    // 获取不同商品的种类数量 (SKU数)
    size_t getDistinctCount() const { return items.size(); }

    // 获取所有商品的总件数 (例如 2台电脑 + 3瓶水 = 5件)
    int getTotalQuantity() const;

    // ========================================================
    // 2. 单项检索与查询
    // ========================================================
    // 判断购物车中是否存在指定商品
    bool hasItem(const std::string& productId) const { return items.find(productId) != items.end(); }

    // 获取指定商品的加购件数 (不存在返回 0)
    int getItemQuantity(const std::string& productId) const
    {
        auto it = items.find(productId);
        return it != items.end() ? it->second.quantity : 0;
    }

    // ========================================================
    // 3. 视图导出与外部遍历
    // ========================================================
    // 按加购顺序导出稳定列表 (供 TUI 渲染稳定列表)
    std::vector<CartItem> getItemslist() const;

    // 获取底层 Map 常引用 (只读零拷贝访问)
    const std::unordered_map<std::string, CartItem>& getItemsMap() const { return items; }
    const std::unordered_map<std::string, CartItem>& getItems() const { return items; }

    // ========================================================
    // 4. 金额与品类核算 (供优惠券规划与结算使用)
    // ========================================================
    // 获取未打折的原价总金额
    double getTotalOriginalAmount() const;

    // 获取指定品类的商品小计金额
    double getCategoryAmount(core::ProductCategory category) const;

    // ========================================================
    // 5. 购物车内容增删与修改
    // ========================================================
    // 添加商品进购物车 (非法或数量<=0返回false；已存在则累加数量)
    bool addItem(std::shared_ptr<Product> product, int quantity = 1);

    // 彻底移除指定商品
    bool removeItem(const std::string& productId);

    // 设置指定商品数量 (数量 <= 0 则自动移除)
    bool updateQuantity(const std::string& productId, int quantity);

    // 便捷步进增加数量 (+step)
    bool increaseQuantity(const std::string& productId, int step = 1);

    // 便捷步进减少数量 (-step)，减至 <= 0 自动移除
    bool decreaseQuantity(const std::string& productId, int step = 1);

    // 清空购物车
    void clear();

    // ========================================================
    // 6. 库存校验与原子扣减事务
    // ========================================================
    // 前置库存严格校验 (若有不足返回false，并传出具体错误信息)
    bool validateStockAvailable(std::string& out_error_message) const;

    // 结算批量扣减库存事务 (全项校验通过才实际扣库存，扣完自动清空)
    bool deductAllStock();
};

} // namespace shop
