#include "shop/ProductManager.hpp"

namespace shop
{

bool ProductManager::addProduct(std::shared_ptr<Product> product)
{
    if(!product) return false;
    const auto& id = product->getId();
    return products.try_emplace(id, std::move(product)).second;
}

bool ProductManager::addProduct(std::string id, std::string name, core::ProductCategory category, double price, int stock)
{
    auto prod=std::make_shared<Product>(std::move(id), std::move(name), category, price, stock);
    return addProduct(std::move(prod));
}

bool ProductManager::removeProduct(const std::string& id)
{
    return products.erase(id) > 0;
}

bool ProductManager::replenishProduct(const std::string& id, int count)
{
    auto it = products.find(id);
    if(it == products.end()) return false;
    return it->second->replenishStock(count);
}

bool ProductManager::deductProduct(const std::string& id, int count)
{
    auto it = products.find(id);
    if(it == products.end()) return false;
    return it->second->deductStock(count);
}

bool ProductManager::setStock(const std::string& id, int stock)
{
    auto it = products.find(id);
    if(it == products.end()) return false;
    return it->second->setStock(stock);
}

bool ProductManager::updatePrice(const std::string& id, double price)
{
    auto it = products.find(id);
    if(it == products.end()) return false;
    return it->second->setPrice(price);
}

bool ProductManager::updateName(const std::string& id, const std::string& name)
{
    auto it = products.find(id);
    if(it == products.end()) return false;
    return it->second->setName(name);
}

bool ProductManager::updateCategory(const std::string& id, core::ProductCategory category)
{
    if(category == core::ProductCategory::ALL) return false;
    auto it = products.find(id);
    if(it == products.end()) return false;
    it->second->setCategory(category);
    return true;
}

std::shared_ptr<Product> ProductManager::findProduct(const std::string& id) const
{
    auto it = products.find(id);
    if(it == products.end()) return nullptr;
    return it->second;
}

std::vector<std::shared_ptr<Product>> ProductManager::searchByName(const std::string& name) const
{
    std::vector<std::shared_ptr<Product>> result;
    for(const auto& [id, product] : products)
        if(product->getName().find(name) != std::string::npos)
            result.push_back(product);
    std::sort(result.begin(), result.end(),
        [](const std::shared_ptr<Product>& a, const std::shared_ptr<Product>& b) {
        return a->getId() < b->getId();
    });
    return result;
}

std::vector<std::string> ProductManager::searchByCategory(core::ProductCategory category) const
{
    std::vector<std::string> result;
    for(const auto& [id, product] : products)
        if(category == core::ProductCategory::ALL || product->getCategory() == category)
            result.push_back(id);
    return result;
}

std::vector<std::shared_ptr<Product>> ProductManager::getProducts() const
{
    std::vector<std::shared_ptr<Product>> result(products.size());
    int i=0;
    for(const auto& [id, product] : products)
        result[i++]=product;
    std::sort(result.begin(), result.end(),
        [](const std::shared_ptr<Product>& a, const std::shared_ptr<Product>& b) {
        return a->getId() < b->getId();
    });
    return result;
}

std::vector<std::shared_ptr<Product>> ProductManager::getProductsByCategory(core::ProductCategory category) const
{
    std::vector<std::shared_ptr<Product>> result;
    for(const auto& [id, product] : products)
        if(category==core::ProductCategory::ALL||category == product->getCategory())
            result.push_back(product);
    std::sort(result.begin(), result.end(),
        [](const std::shared_ptr<Product>& a, const std::shared_ptr<Product>& b) {
        return a->getId() < b->getId();
    });
    return result;
}

nlohmann::json ProductManager::toJson() const
{
    nlohmann::json result;
    for(const auto& [id, product] : products)
        result[id]=product->toJson();
    return result;
}

ProductManager ProductManager::fromJson(const nlohmann::json& j)
{
    ProductManager result;
    result.loadFromJson(j);
    return result;
}

bool ProductManager::loadFromJson(const nlohmann::json& j)
{
    try
    {
        if(!j.is_object())
        {
            return false;
        }

        clear();
        for(const auto& [id, product] : j.items())
        {
            auto prod = Product::fromJson(product);
            if(prod)
            {
                products[prod->getId()] = prod;
            }
        }
        return true;
    }
    catch(const std::exception&)
    {
        return false;
    }
}

}