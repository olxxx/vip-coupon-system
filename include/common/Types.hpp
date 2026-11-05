#pragma once
#include <string>
#include <chrono>
#include <ctime>

namespace core
{

// 会员等级枚举 (V0 ~ V5)
enum class UserTier
{
    V0_NORMAL=0,    // 普通用户: 基础无折扣
    V1_BRONZE=1,    // 青铜会员: 98折 (0.98)
    V2_SILVER=2,    // 白银会员: 95折 (0.95)
    V3_GOLD=3,      // 黄金会员: 90折 (0.90)
    V4_DIAMOND=4,   // 钻石会员: 85折 (0.85)
    V5_SUPREME=5    // 至尊会员: 80折 (0.80)
};

// 会员等级元数据配置 (数据驱动模式)
struct TierConfig
{
    UserTier tier;
    std::string name;          // 等级中文名称
    double discount_rate;      // 基础全场折扣 (如 0.95)
    double upgrade_threshold;  // 晋升到该等级所需的累计总消费额 (元)
};

// 根据等级枚举获取配置信息 (避免为纯数据写 6 个派生类)
inline TierConfig getTierConfig(UserTier tier)
{
    switch(tier)
    {
        case UserTier::V0_NORMAL:	return {tier,"普通会员",1.00,0.0};
        case UserTier::V1_BRONZE:	return {tier,"青铜会员",0.98,200.0};
        case UserTier::V2_SILVER:	return {tier,"白银会员",0.95,800.0};
        case UserTier::V3_GOLD:	    return {tier,"黄金会员",0.90,2000.0};
        case UserTier::V4_DIAMOND:	return {tier,"钻石会员",0.85,5000.0};
        case UserTier::V5_SUPREME:	return {tier,"至尊会员",0.80,10000.0};
    }
    return {UserTier::V0_NORMAL,"普通会员",1.00,0.0};
}

// 账户身份类型
enum class AccountType
{
    CUSTOMER=0,     // 消费者前台账户
    ADMIN=1         // 后台管理账户
};

// 管理员权限角色 (基于 RBAC 的两级安全划分)
enum class AdminRole
{
    STAFF=0,        // 运营管理员: 拥有商品上架/补货、发布优惠券、查看列表权限
    SUPER_ADMIN=1   // 超级管理员: 拥有全部权限，包含直接修改用户等级、系统重置
};

// 商品品类划分
enum class ProductCategory
{
    ALL=0,          // 全品类通用 (专供优惠券使用：表示全场适用)
    DIGITAL=1,      // 数码 3C
    CLOTHING=2,     // 服饰鞋包
    FOOD=3,         // 食品生鲜
    DAILY=4,        // 日用百货
    OTHER=5         // 其他
};

inline std::string getCategoryName(ProductCategory cat)
{
    switch(cat)
    {
        case ProductCategory::ALL:      return "全品类通用";
        case ProductCategory::DIGITAL:	return "数码3C";
        case ProductCategory::CLOTHING:	return "服饰鞋包";
        case ProductCategory::FOOD:	    return "食品生鲜";
        case ProductCategory::DAILY:	return "日用百货";
        case ProductCategory::OTHER:	return "其他";
    }
    return "未知分类";
}

// 时间生效约束类型 (针对优惠券)
enum class TimeConstraint
{
    ANYTIME=0,      // 不限时间
    WEEKDAY_ONLY=1, // 仅限工作日生效 (周一至周五)
    WEEKEND_ONLY=2  // 仅限周末生效 (周六至周日)
};

// 星期枚举
enum class DayOfWeek
{
    MONDAY=1,
    TUESDAY=2,
    WEDNESDAY=3,
    THURSDAY=4,
    FRIDAY=5,
    SATURDAY=6,
    SUNDAY=7
};

inline bool isWeekend(DayOfWeek day)
{
    return day==DayOfWeek::SATURDAY||day==DayOfWeek::SUNDAY;
}

// 浮点数金额安全比较 (防止 199.99999999 < 200.0 导致的偶发门槛未达标 Bug)
inline bool isAmountGTE(double a,double b)
{
    return a>=b-1e-4;
}

// 获取当前系统标准格式化时间戳 (YYYY-MM-DD HH:MM:SS)
inline std::string getCurrentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &raw_time);
#else
    localtime_r(&raw_time, &local_time);
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_time);
    return buffer;
}

} // namespace core
