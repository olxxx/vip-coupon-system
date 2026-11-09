#include "shop/ProductManager.hpp"

namespace shop
{

bool ProductManager::addProduct(std::shared_ptr<Product> product)
{
    if(!product) return false;
    if(hasProduct(product->getId())) return false;
    products[product->getId()]=product;
    return true;
}

bool ProductManager::addProduct(std::string id, std::string name, core::ProductCategory category, double price, int stock)
{
    auto prod=std::make_shared<Product>(id, name, category, price, stock);
    return addProduct(prod);
}

bool ProductManager::removeProduct(const std::string& id)
{
    if(!hasProduct(id)) return false;
    products.erase(id);
    return true;
}

bool ProductManager::replenishProduct(const std::string& id, int count)
{
    if(!hasProduct(id)) return false;
    return products[id]->replenishStock(count);
}

bool ProductManager::deductProduct(const std::string& id, int count)
{
    if(!hasProduct(id)) return false;
    return products[id]->deductStock(count);
}

bool ProductManager::setStock(const std::string& id, int stock)
{
    if(!hasProduct(id)) return false;
    return products[id]->setStock(stock);
}

bool ProductManager::updatePrice(const std::string& id, double price)
{
    if(!hasProduct(id)) return false;
    return products[id]->setPrice(price);
}

bool ProductManager::updateName(const std::string& id, const std::string& name)
{
    if(!hasProduct(id)) return false;
    return products[id]->setName(name);
}

bool ProductManager::updateCategory(const std::string& id, core::ProductCategory category)
{
    if(!hasProduct(id) || category == core::ProductCategory::ALL) return false;
    products[id]->setCategory(category);
    return true;
}

std::shared_ptr<Product> ProductManager::findProduct(const std::string& id) const
{
    if(!hasProduct(id)) return nullptr;
    return products.at(id);
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