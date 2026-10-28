#pragma once
#include "common/Types.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace shop
{

class Product
{
private:
    std::string id;
    std::string name;
    core::ProductCategory category;
    double price;
    int stock;
public:
    Product();
    Product(std::string _id,std::string _name,core::ProductCategory _category,double _price,int _stock);
        
    virtual ~Product() {}
    
    //Getters
    std::string getId() const { return id; }
    std::string getName() const { return name; }
    core::ProductCategory getCategory() const { return category; }
    double getPrice() const { return price; }
    int getStock() const { return stock; }
    bool hasStock(int count=1) const { return count>0&&stock>=count; }
    bool isOutOfStock() const { return stock <= 0; }

    //Setters
    bool deductStock(int count);
    bool replenishStock(int count);
    bool setStock(int count);
    bool setPrice(double num);
    bool setName(std::string str);
    void setCategory(core::ProductCategory _category);

    //UI
    std::string getCategoryName() const;
    std::string getFormattedPrice() const;
    std::string getDisplaySummary() const;

    //Operators
    bool operator==(const Product& other) const;

    //JSON
    nlohmann::json toJson() const;
    static Product fromJson(const nlohmann::json& j);
};

} // namespace shop