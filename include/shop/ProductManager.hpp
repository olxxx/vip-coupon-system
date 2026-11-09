#pragma once
#include "common/Types.hpp"
#include "shop/Product.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <nlohmann/json.hpp>

namespace shop
{

class ProductManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<Product>> products;

public:
    ProductManager()=default;

    //规模与状态
    size_t size() const { return products.size(); }
    bool empty() const { return products.empty(); }
    bool hasProduct(const std::string& id) const { return products.find(id) != products.end(); }
    void clear() { products.clear(); }

    //商品出入与修改
    bool addProduct(std::shared_ptr<Product> product);
    bool addProduct(std::string id,std::string name,core::ProductCategory category,double price,int stock);
    bool removeProduct(const std::string& id);
    bool replenishProduct(const std::string& id, int count);
    bool deductProduct(const std::string& id, int count);
    bool setStock(const std::string& id, int stock);
    bool updatePrice(const std::string& id, double price);
    bool updateName(const std::string& id, const std::string& name);
    bool updateCategory(const std::string& id, core::ProductCategory category);

    //检索查询
    std::shared_ptr<Product> findProduct(const std::string& id) const;
    std::vector<std::shared_ptr<Product>> searchByName(const std::string& name) const;
    std::vector<std::string> searchByCategory(core::ProductCategory category) const;

    //视图导出
    std::vector<std::shared_ptr<Product>> getProducts() const;
    std::vector<std::shared_ptr<Product>> getProductsByCategory(core::ProductCategory category) const;
    const std::unordered_map<std::string, std::shared_ptr<Product>>& getProductMap() const { return products; }

    //JSON序列化与持久化
    nlohmann::json toJson() const;
    static ProductManager fromJson(const nlohmann::json& j);
    bool loadFromJson(const nlohmann::json& j);
};

}
