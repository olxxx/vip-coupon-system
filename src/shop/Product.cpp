#include "shop/Product.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace shop
{

Product::Product() : id(""), name(""), category(core::ProductCategory::OTHER), price(0.0), stock(0) {}

Product::Product(std::string _id, std::string _name, core::ProductCategory _category, double _price, int _stock):
                id(std::move(_id)), name(std::move(_name)), category(_category), price(std::max(0.0,_price)), stock(std::max(0,_stock)) {}

bool Product::deductStock(int count)
{
    if(count<=0||count>stock) return false;
    stock-=count;
    return true;
}

bool Product::replenishStock(int count)
{
    if(count<=0) return false;
    stock+=count;
    return true;
}

bool Product::setStock(int count)
{
    if(count<0) return false;
    stock=count;
    return true;
}

bool Product::setPrice(double num)
{
    if(num<0) return false;
    price=num;
    return true;
}

bool Product::setName(std::string str)
{
    if(str.empty()) return false;
    name=str;
    return true;
}

void Product::setCategory(core::ProductCategory _category)
{
    category=_category;
}

std::string Product::getCategoryName() const
{
    return core::getCategoryName(category);
}

std::string Product::getFormattedPrice() const
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << price;
    return oss.str();
}

std::string Product::getDisplaySummary() const
{
    std::ostringstream oss;
    oss << "商品ID: " << id << "\n"
        << "名称: " << name << "\n"
        << "分类: " << getCategoryName() << "\n"
        << "价格: ¥" << getFormattedPrice() << "\n"
        << "库存: " << stock;
    return oss.str();
}

bool Product::operator==(const Product& other) const
{
    return id == other.id;
}

nlohmann::json Product::toJson() const
{
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["category"] = static_cast<int>(category);
    j["price"] = price;
    j["stock"] = stock;
    return j;
}

Product Product::fromJson(const nlohmann::json& j)
{
    std::string id = j.at("id").get<std::string>();
    std::string name = j.at("name").get<std::string>();
    core::ProductCategory category = static_cast<core::ProductCategory>(j.at("category").get<int>());
    double price = j.at("price").get<double>();
    int stock = j.at("stock").get<int>();

    return Product(id, name, category, price, stock);
}

};