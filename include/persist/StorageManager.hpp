#pragma once
#include "user/UserManager.hpp"
#include "shop/ProductManager.hpp"
#include "coupon/CouponManager.hpp"

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace persist
{

class StorageManager
{
private:
    std::filesystem::path data_dir;
    std::filesystem::path users_file;
    std::filesystem::path products_file;
    std::filesystem::path coupons_file;

    void seedDefaultData(user::UserManager& user_mgr,
                         shop::ProductManager& prod_mgr,
                         shop::CouponManager& cpn_mgr);
    bool writeJsonToFile(const std::filesystem::path& filepath, const nlohmann::json& json_data) const;
    bool readJsonFromFile(const std::filesystem::path& filepath, nlohmann::json& out_json) const;

public:
    explicit StorageManager(std::filesystem::path data_dir = std::filesystem::path("data"));

    //冷启动与状态
    void initializeDataDirIfEmpty(user::UserManager& user_mgr,
                                  shop::ProductManager& prod_mgr,
                                  shop::CouponManager& cpn_mgr);
    bool isDataInitialized() const;
    bool resetToDefault(user::UserManager& user_mgr,
                        shop::ProductManager& prod_mgr,
                        shop::CouponManager& cpn_mgr);

    //全量存取
    bool loadAll(user::UserManager& user_mgr,
                 shop::ProductManager& prod_mgr,
                 shop::CouponManager& cpn_mgr);
    bool saveAll(const user::UserManager& user_mgr,
                 const shop::ProductManager& prod_mgr,
                 const shop::CouponManager& cpn_mgr);

    //单项存取
    bool saveUsers(const user::UserManager& user_mgr);
    bool saveProducts(const shop::ProductManager& prod_mgr);
    bool saveCoupons(const shop::CouponManager& cpn_mgr);
    bool loadUsers(user::UserManager& user_mgr);
    bool loadProducts(shop::ProductManager& prod_mgr);
    bool loadCoupons(shop::CouponManager& cpn_mgr);

    //路径检索
    const std::filesystem::path& getDataDir() const { return data_dir; }
    const std::filesystem::path& getUsersFilePath() const { return users_file; }
    const std::filesystem::path& getProductsFilePath() const { return products_file; }
    const std::filesystem::path& getCouponsFilePath() const { return coupons_file; }
};

} // namespace persist
