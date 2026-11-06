#include "persist/StorageManager.hpp"
#include "coupon/ConcreteCoupons.hpp"

#include <fstream>
#include <iomanip>
#include <algorithm>

namespace persist
{

namespace {

// 辅助函数：根据运行环境进行智能路径自适应探测
std::filesystem::path resolveAdaptiveDataDir(std::filesystem::path target_dir)
{
    // 仅当用户使用默认的 "data" 相对路径时才触发自适应探测
    // 若显式传入如 "tests/data" 或绝对路径，则完全尊重入参
    if (target_dir == "data" || target_dir == "data/")
    {
        // 场景 1: 从项目根目录启动运行 (当前工作目录下存在 bin/ 且存在 bin/data/ 目录或已有数据)
        if (std::filesystem::exists("bin/data"))
        {
            return std::filesystem::path("bin/data");
        }
        // 场景 2: 从项目根目录首次冷启动 (存在 bin/ 目录但 bin/data/ 尚未建立)
        if (std::filesystem::exists("bin") && std::filesystem::is_directory("bin"))
        {
            return std::filesystem::path("bin/data");
        }
    }
    return target_dir;
}

} // namespace

StorageManager::StorageManager(std::filesystem::path dir)
    : data_dir(resolveAdaptiveDataDir(std::move(dir))),
      users_file(data_dir / "users.json"),
      products_file(data_dir / "products.json"),
      coupons_file(data_dir / "coupons.json")
{
}

// -----------------------------------------------------------------------------
// 1. 冷启动与状态管理
// -----------------------------------------------------------------------------

bool StorageManager::isDataInitialized() const
{
    return std::filesystem::exists(users_file) &&
           std::filesystem::exists(products_file) &&
           std::filesystem::exists(coupons_file);
}

void StorageManager::initializeDataDirIfEmpty(user::UserManager& user_mgr,
                                              shop::ProductManager& prod_mgr,
                                              shop::CouponManager& cpn_mgr)
{
    if (isDataInitialized())
    {
        loadAll(user_mgr, prod_mgr, cpn_mgr);
        return;
    }

    // 目录或任意核心文件缺失，执行冷启动种子数据注入并落盘
    seedDefaultData(user_mgr, prod_mgr, cpn_mgr);
    saveAll(user_mgr, prod_mgr, cpn_mgr);
}

bool StorageManager::resetToDefault(user::UserManager& user_mgr,
                                    shop::ProductManager& prod_mgr,
                                    shop::CouponManager& cpn_mgr)
{
    user_mgr.clear();
    prod_mgr.clear();
    cpn_mgr.clear();

    seedDefaultData(user_mgr, prod_mgr, cpn_mgr);
    return saveAll(user_mgr, prod_mgr, cpn_mgr);
}

// -----------------------------------------------------------------------------
// 2. 全量存取调度
// -----------------------------------------------------------------------------

bool StorageManager::loadAll(user::UserManager& user_mgr,
                             shop::ProductManager& prod_mgr,
                             shop::CouponManager& cpn_mgr)
{
    bool ok_users = loadUsers(user_mgr);
    bool ok_prods = loadProducts(prod_mgr);
    bool ok_cpns  = loadCoupons(cpn_mgr);
    return ok_users && ok_prods && ok_cpns;
}

bool StorageManager::saveAll(const user::UserManager& user_mgr,
                             const shop::ProductManager& prod_mgr,
                             const shop::CouponManager& cpn_mgr)
{
    bool ok_users = saveUsers(user_mgr);
    bool ok_prods = saveProducts(prod_mgr);
    bool ok_cpns  = saveCoupons(cpn_mgr);
    return ok_users && ok_prods && ok_cpns;
}

// -----------------------------------------------------------------------------
// 3. 单项模块存取
// -----------------------------------------------------------------------------

bool StorageManager::saveUsers(const user::UserManager& user_mgr)
{
    return writeJsonToFile(users_file, user_mgr.toJson());
}

bool StorageManager::saveProducts(const shop::ProductManager& prod_mgr)
{
    return writeJsonToFile(products_file, prod_mgr.toJson());
}

bool StorageManager::saveCoupons(const shop::CouponManager& cpn_mgr)
{
    return writeJsonToFile(coupons_file, cpn_mgr.toJson());
}

bool StorageManager::loadUsers(user::UserManager& user_mgr)
{
    nlohmann::json j;
    if (!readJsonFromFile(users_file, j))
        return false;
    return user_mgr.loadFromJson(j);
}

bool StorageManager::loadProducts(shop::ProductManager& prod_mgr)
{
    nlohmann::json j;
    if (!readJsonFromFile(products_file, j))
        return false;
    return prod_mgr.loadFromJson(j);
}

bool StorageManager::loadCoupons(shop::CouponManager& cpn_mgr)
{
    nlohmann::json j;
    if (!readJsonFromFile(coupons_file, j))
        return false;
    return cpn_mgr.loadFromJson(j);
}

// -----------------------------------------------------------------------------
// 4. 私有辅助方法与种子注入
// -----------------------------------------------------------------------------

bool StorageManager::writeJsonToFile(const std::filesystem::path& filepath, const nlohmann::json& json_data) const
{
    try
    {
        // 自动创建多级父目录
        if (filepath.has_parent_path())
        {
            std::filesystem::create_directories(filepath.parent_path());
        }

        std::ofstream file(filepath, std::ios::out | std::ios::trunc);
        if (!file.is_open())
            return false;

        file << std::setw(4) << json_data << std::endl;
        return static_cast<bool>(file);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool StorageManager::readJsonFromFile(const std::filesystem::path& filepath, nlohmann::json& out_json) const
{
    try
    {
        if (!std::filesystem::exists(filepath))
            return false;

        std::ifstream file(filepath, std::ios::in);
        if (!file.is_open())
            return false;

        file >> out_json;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void StorageManager::seedDefaultData(user::UserManager& user_mgr,
                                     shop::ProductManager& prod_mgr,
                                     shop::CouponManager& cpn_mgr)
{
    // A. 注入默认账号
    user_mgr.registerAdmin("root", "admin123", core::AdminRole::SUPER_ADMIN);
    user_mgr.registerAdmin("staff", "staff123", core::AdminRole::STAFF);

    // 默认测试消费者：余额 2000.0
    auto demo_user = std::make_shared<user::CustomerAccount>(
        "demo", "123456", core::getCurrentTimestamp(), 2000.0
    );
    user_mgr.loadCustomerDirect(demo_user);

    // B. 注入典型商品库 (覆盖品类、高低库存与售罄)
    prod_mgr.addProduct("P1001", "机械键盘", core::ProductCategory::DIGITAL, 399.0, 50);
    prod_mgr.addProduct("P1002", "轻薄笔记本电脑", core::ProductCategory::DIGITAL, 5999.0, 20);
    prod_mgr.addProduct("P1003", "入耳式无线耳机", core::ProductCategory::DIGITAL, 299.0, 100);
    prod_mgr.addProduct("P2001", "连帽工装夹克", core::ProductCategory::CLOTHING, 259.0, 30);
    prod_mgr.addProduct("P2002", "纯棉休闲T恤", core::ProductCategory::CLOTHING, 89.0, 80);
    prod_mgr.addProduct("P3001", "精选混合坚果礼盒", core::ProductCategory::FOOD, 128.0, 40);
    prod_mgr.addProduct("P3002", "原味有机牛奶整箱", core::ProductCategory::FOOD, 65.0, 60);
    prod_mgr.addProduct("P4001", "不锈钢真空保温杯", core::ProductCategory::DAILY, 79.0, 50);
    prod_mgr.addProduct("P4002", "深层洁净洗衣凝珠", core::ProductCategory::DAILY, 49.0, 5);
    prod_mgr.addProduct("P5001", "限量版纪念手办", core::ProductCategory::OTHER, 499.0, 0); // 售罄测试项

    // C. 注入全套预设优惠券模板 (满减、折扣、立减；免费券与付费券；品类限制与时间限制)
    // 1. 经典无门槛立减券 (免费)
    cpn_mgr.registerCouponToPool(std::make_shared<shop::CashCoupon>(
        "TPL_CASH_05", "新用户无门槛红包", 0.0, 5.0,
        core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
        core::TimeConstraint::ANYTIME
    ));

    // 2. 全场通用满减券 (免费)
    cpn_mgr.registerCouponToPool(std::make_shared<shop::ThresholdCoupon>(
        "TPL_THRESHOLD_200_30", "全场畅选满200减30", 0.0, 200.0,
        core::ProductCategory::ALL, core::UserTier::V0_NORMAL,
        core::TimeConstraint::ANYTIME, 30.0
    ));

    // 3. 数码专属周末大额满减券 (付费 10 元购买，需青铜 V1 会员)
    cpn_mgr.registerCouponToPool(std::make_shared<shop::ThresholdCoupon>(
        "TPL_DIGITAL_WEEKEND_100", "周末数码特惠满1000减150", 10.0, 1000.0,
        core::ProductCategory::DIGITAL, core::UserTier::V1_BRONZE,
        core::TimeConstraint::WEEKEND_ONLY, 150.0
    ));

    // 4. 食品生鲜品类 85 折扣券 (免费，工作日可用)
    cpn_mgr.registerCouponToPool(std::make_shared<shop::DiscountCoupon>(
        "TPL_FOOD_DISCOUNT_85", "工作日美食85折券", 0.0, 100.0, 0.85,
        core::ProductCategory::FOOD, core::UserTier::V0_NORMAL,
        core::TimeConstraint::WEEKDAY_ONLY, 50.0
    ));

    // 5. 尊享会员 9 折大券 (付费 20 元购买，需黄金 V3 会员，最高减 300)
    cpn_mgr.registerCouponToPool(std::make_shared<shop::DiscountCoupon>(
        "TPL_VIP_DISCOUNT_90", "黄金专享全场9折券", 20.0, 500.0, 0.90,
        core::ProductCategory::ALL, core::UserTier::V3_GOLD,
        core::TimeConstraint::ANYTIME, 300.0
    ));
}

} // namespace persist
