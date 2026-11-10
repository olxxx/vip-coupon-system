#include "shop/Cart.hpp"
#include <algorithm>

namespace shop
{

int Cart::getTotalQuantity() const
{
    int total=0;
    for(const auto& [id,item]:items) total+=item.quantity;
    return total;
}

std::vector<CartItem> Cart::getItemslist() const
{
    std::vector<CartItem> result;
    for(const std::string& id:item_order) result.push_back(items.at(id));
    return result;
}

double Cart::getTotalOriginalAmount() const
{
    double total=0;
    for(const auto& [id,item]:items)
        total+=item.quantity*item.product->getPrice();
    return total;
}

double Cart::getCategoryAmount(core::ProductCategory category) const
{
    if(category==core::ProductCategory::ALL)
        return getTotalOriginalAmount();

    double total=0;
    for(const auto& [id,item]:items)
        if(item.product->getCategory()==category) total+=item.quantity*item.product->getPrice();
    return total;
}

bool Cart::addItem(std::shared_ptr<Product> product, int quantity)
{
    if(quantity<=0) return false;
    if(!product) return false;

    int current_qty = getItemQuantity(product->getId());
    if(!product->hasStock(current_qty+quantity)) return false;

    if(hasItem(product->getId()))
    {
        items[product->getId()].quantity+=quantity;
        return true;
    }
    items[product->getId()] = CartItem{product, quantity};
    item_order.push_back(product->getId());
    return true;
}

bool Cart::removeItem(const std::string& productId)
{
    if(!hasItem(productId)) return false;
    items.erase(productId);
    item_order.erase(std::remove(item_order.begin(),item_order.end(),productId),item_order.end());
    return true;
}

bool Cart::updateQuantity(const std::string& productId, int quantity)
{
    if(!hasItem(productId)||quantity<0) return false;
    if(quantity==0)
        return removeItem(productId);
    if(!items[productId].product->hasStock(quantity)) return false;
    items[productId].quantity=quantity;
    return true;
}

bool Cart::increaseQuantity(const std::string& productId, int step)
{
    if(!hasItem(productId)||step<0) return false;
    if(!items[productId].product->hasStock(items[productId].quantity+step)) return false;
    items[productId].quantity+=step;
    return true;
}

bool Cart::decreaseQuantity(const std::string& productId, int step)
{
    if(!hasItem(productId)||step<0||items[productId].quantity-step<0) return false;
    items[productId].quantity-=step;
    if(items[productId].quantity==0)
        removeItem(productId);
    return true;
}

void Cart::clear()
{
    items.clear();
    item_order.clear();
}

bool Cart::validateStockAvailable(std::string& out_error_message) const
{
    for(const auto& [productId, item] : items)
    {
        if(!item.product->hasStock(item.quantity))
        {
            out_error_message = "Product " + item.product->getName() + " is out of stock.";
            return false;
        }
    }
    return true;
}

bool Cart::deductAllStock()
{
    std::string out_error_message;
    if(!validateStockAvailable(out_error_message)) return false;

    for(const auto& [productId, item] : items)
        if(!item.product->deductStock(item.quantity))
            return false;
    clear();
    return true;
}



}
