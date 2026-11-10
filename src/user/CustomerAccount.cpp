#include "user/CustomerAccount.hpp"

#include "persist/JsonFactories.hpp"

#include <algorithm>
#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

namespace user {

    //放在匿名空间的辅助函数
    namespace {

        core::UserTier parseTier(int value) {
            if (value < static_cast<int>(core::UserTier::V0_NORMAL)
                || value > static_cast<int>(core::UserTier::V5_SUPREME)) {
                return core::UserTier::V0_NORMAL;
            }

            return static_cast<core::UserTier>(value);//整数转枚举
        }//core::UserTier在include/common/Types.hpp中

    } // namespace


    //构造函数-----注册新用户-----
    CustomerAccount::CustomerAccount(std::string name,
        std::string password,
        std::string creation_time,
        double initial_balance)
        : Account(
            std::move(name),
            std::move(password),
            std::move(creation_time)),
        balance(std::max(0.0, initial_balance)) {
    }

    //构造函数-----读取存档-----
    CustomerAccount::CustomerAccount(std::string name,
        std::string password,
        std::string creation_time,
        double restored_balance,
        double restored_total_spent,
        core::UserTier restored_tier)
        : Account(
            std::move(name),
            std::move(password),
            std::move(creation_time)),
        balance(std::max(0.0, restored_balance)),
        total_spent(std::max(0.0, restored_total_spent)),
        tier(restored_tier) {
    }

    core::AccountType CustomerAccount::getAccountType() const {
        return core::AccountType::CUSTOMER;
    }//纯虚函数，返回账户类型：消费者

    std::string CustomerAccount::getDisplaySummary() const {
        std::ostringstream stream;
        stream << getUsername()
            << " | "
            << getTierName()
            << " | 余额：¥"
            << std::fixed
            << std::setprecision(2)
            << balance;
        return stream.str();
    }//生成账户summary，eg:  gyc | 白银会员 | 余额：¥850.50

    double CustomerAccount::getBalance() const {
        return balance;
    }

    double CustomerAccount::getTotalSpent() const {
        return total_spent;
    }

    core::UserTier CustomerAccount::getTier() const {
        return tier;
    }

    std::string CustomerAccount::getTierName() const {
        return core::getTierConfig(tier).name;
    }

    double CustomerAccount::getDiscountRate() const {
        return core::getTierConfig(tier).discount_rate;
    }

    void CustomerAccount::setBalance(double new_balance) {
        balance = std::max(0.0, new_balance);
    }

    void CustomerAccount::setTotalSpent(double new_total_spent) {
        total_spent = std::max(0.0, new_total_spent);
    }

    void CustomerAccount::recharge(
        double amount,
        const std::string& channel
    ) {
        if (amount <= 1e-4) {
            return;
        }

        balance += amount;

        addTransaction(std::make_unique<RechargeRecord>(
            TransactionRecord::generateId("RECHARGE"),
            core::getCurrentTimestamp(),
            amount,
            channel
        ));
    }//recharge:充值余额

    bool CustomerAccount::deductBalance(
        double amount,
        const std::string& order_id
    ) {
        if (amount < 0.0
            || !core::isAmountGTE(balance, amount)) {
            return false;
        }

        balance -= amount;
        if (balance < 0.0 && balance > -1e-4) {
            balance = 0.0;
        }

        total_spent += amount;

        addTransaction(std::make_unique<ExpenseRecord>(
            TransactionRecord::generateId("EXPENSE"),
            core::getCurrentTimestamp(),
            amount,
            order_id,
            "购买商品结算"
        ));

        std::string upgrade_message;
        checkAndPerformUpgrade(upgrade_message);
        return true;
    }//购物结算余额

    bool CustomerAccount::deductForCoupon(
        double amount,
        const std::string& coupon_name,
        const std::string& template_id
    ) {
        if (amount <= 1e-4
            || !core::isAmountGTE(balance, amount)) {
            return false;
        }

        balance -= amount;
        if (balance < 0.0 && balance > -1e-4) {
            balance = 0.0;
        }

        addTransaction(std::make_unique<PurchaseCouponRecord>(
            TransactionRecord::generateId("COUPON"),
            core::getCurrentTimestamp(),
            amount,
            template_id,
            coupon_name
        ));

        return true;
    }//购买优惠券结算余额

    bool CustomerAccount::checkAndPerformUpgrade(
        std::string& out_upgrade_message
    ) {
        out_upgrade_message.clear();

        if (tier == core::UserTier::V5_SUPREME) {
            return false;
        }

        core::UserTier new_tier = tier;

        for (int value = static_cast<int>(tier) + 1;
            value <= static_cast<int>(core::UserTier::V5_SUPREME);
            ++value) {
            const auto candidate =
                static_cast<core::UserTier>(value);
            const double required =
                core::getTierConfig(candidate).upgrade_threshold;

            if (!core::isAmountGTE(total_spent, required)) {
                break;
            }

            new_tier = candidate;
        }

        if (new_tier == tier) {
            return false;
        }

        const std::string old_name =
            core::getTierConfig(tier).name;
        const std::string new_name =
            core::getTierConfig(new_tier).name;

        tier = new_tier;

        addTransaction(std::make_unique<UpgradeRecord>(
            TransactionRecord::generateId("UPGRADE"),
            core::getCurrentTimestamp(),
            old_name,
            new_name
        ));

        out_upgrade_message =
            "恭喜晋升为" + new_name + "！";

        return true;
    }//是否升级？

    void CustomerAccount::forceSetTier(core::UserTier new_tier) {
        const int tier_value = static_cast<int>(new_tier);
        if (tier_value < static_cast<int>(core::UserTier::V0_NORMAL)
            || tier_value > static_cast<int>(core::UserTier::V5_SUPREME)
            || new_tier == tier) {
            return;
        }

        const std::string old_name =
            core::getTierConfig(tier).name;
        const std::string new_name =
            core::getTierConfig(new_tier).name;

        tier = new_tier;

        //保存管理员操作流水
        addTransaction(std::make_unique<UpgradeRecord>(
            TransactionRecord::generateId("ADMIN_TIER"),
            core::getCurrentTimestamp(),
            old_name,
            new_name
        ));
    }//用于管理员强制修改用户的会员等级，不依赖累计消费金额。

    double CustomerAccount::getNextTierThreshold() const {
        if (isMaxTier()) {
            return core::getTierConfig(tier).upgrade_threshold;
        }

        const auto next_tier =
            static_cast<core::UserTier>(
                static_cast<int>(tier) + 1
                );

        return core::getTierConfig(next_tier).upgrade_threshold;
    }//读取到达下一级会员的门槛

    double CustomerAccount::getGrowthProgress() const {
        if (isMaxTier()) {
            return 1.0;
        }

        const double current_threshold =
            core::getTierConfig(tier).upgrade_threshold;
        const double next_threshold = getNextTierThreshold();

        if (next_threshold <= current_threshold + 1e-4) {
            return 1.0;
        }

        const double progress =
            (total_spent - current_threshold)
            / (next_threshold - current_threshold);

        return std::clamp(progress, 0.0, 1.0);
    }//生成一个0-1的小数，提供进度条

    double CustomerAccount::getAmountNeededForNextTier() const {
        if (isMaxTier()) {
            return 0.0;
        }

        return std::max(
            0.0,
            getNextTierThreshold() - total_spent
        );
    }//计算:距离下一级会员还要多少钱

    bool CustomerAccount::isMaxTier() const {
        return tier == core::UserTier::V5_SUPREME;
    }

    void CustomerAccount::addCoupon(
        std::shared_ptr<shop::Coupon> coupon
    ) {
        if (!coupon || coupon->getInstanceId().empty()) {
            return;
        }

        if (hasCoupon(coupon->getInstanceId())) {
            return;
        }

        coupons.push_back(std::move(coupon));
    }//添加优惠券

    std::shared_ptr<shop::Coupon> CustomerAccount::findCoupon(
        const std::string& instance_id
    ) const {
        const auto found = std::find_if(
            coupons.begin(),
            coupons.end(),
            [&instance_id](const auto& coupon) {
                return coupon
                    && coupon->getInstanceId() == instance_id;
            }
        );

        return found == coupons.end() ? nullptr : *found;
    }//查找优惠券


    bool CustomerAccount::consumeCoupon(
        const std::string& instance_id
    ) {
        const auto coupon = findCoupon(instance_id);

        if (!coupon || coupon->isUsed()) {
            return false;
        }

        coupon->markAsUsed();
        return true;
    }//把用户背包中的一张优惠券标记为已使用。(核销)

    bool CustomerAccount::removeCoupon(
        const std::string& instance_id
    ) {
        if (instance_id.empty()) {
            return false;
        }

        const auto it = std::find_if(
            coupons.begin(),
            coupons.end(),
            [&instance_id](const auto& coupon) {
                return coupon
                    && coupon->getInstanceId() == instance_id;
            }
        );

        if (it == coupons.end()) {
            return false;
        }

        coupons.erase(it);
        return true;
    }//从用户背包中移除指定实例优惠券

    bool CustomerAccount::hasCoupon(
        const std::string& instance_id
    ) const {
        return findCoupon(instance_id) != nullptr;
    }

    const std::vector<std::shared_ptr<shop::Coupon>>&
        CustomerAccount::getCoupons() const {
        return coupons;
    }

    std::vector<std::shared_ptr<shop::Coupon>>
        CustomerAccount::getAvailableCoupons() const {
        std::vector<std::shared_ptr<shop::Coupon>> result;

        for (const auto& coupon : coupons) {
            if (coupon && !coupon->isUsed()) {
                result.push_back(coupon);
            }
        }

        return result;
    }//看还没用的券

    std::vector<std::shared_ptr<shop::Coupon>>
        CustomerAccount::getUsedCoupons() const {
        std::vector<std::shared_ptr<shop::Coupon>> result;

        for (const auto& coupon : coupons) {
            if (coupon && coupon->isUsed()) {
                result.push_back(coupon);
            }
        }

        return result;
    }//看使用过的券

    std::size_t CustomerAccount::getCouponCount() const {
        return coupons.size();
    }

    std::size_t
        CustomerAccount::getAvailableCouponCount() const {
        return static_cast<std::size_t>(std::count_if(
            coupons.begin(),
            coupons.end(),
            [](const auto& coupon) {
                return coupon && !coupon->isUsed();
            }
        ));
    }//获取未使用的优惠券数量

    void CustomerAccount::cleanUsedCoupons() {
        coupons.erase(
            std::remove_if(
                coupons.begin(),
                coupons.end(),
                [](const auto& coupon) {
                    return !coupon || coupon->isUsed();
                }
            ),
            coupons.end()
        );
    }

    void CustomerAccount::clearCoupons() {
        coupons.clear();
    }

    void CustomerAccount::addTransaction(
        std::unique_ptr<TransactionRecord> record
    ) {
        if (record) {
            transactions.push_back(std::move(record));
        }
    }//加入一条账单流水

    const std::vector<std::unique_ptr<TransactionRecord>>&
        CustomerAccount::getTransactions() const {
        return transactions;
    }

    std::size_t CustomerAccount::getTransactionCount() const {
        return transactions.size();
    }//流水数

    std::vector<const TransactionRecord*>
        CustomerAccount::getRecentTransactions(
            std::size_t limit//limit限制流水数
        ) const {
        std::vector<const TransactionRecord*> result;

        if (limit == 0) {
            return result;
        }

        const std::size_t count =
            std::min(limit, transactions.size());

        for (std::size_t index = transactions.size();
            index > transactions.size() - count;
            --index) {
            if (transactions[index - 1]) {
                result.push_back(transactions[index - 1].get());
            }
        }

        return result;
    }//查看最近的账单流水



    //----------toJson()：把内存中的账户对象转换成 JSON；fromJson()：从 JSON 恢复账户对象----------
    nlohmann::json CustomerAccount::toJson() const {
        nlohmann::json json = toJsonBase();

        json["balance"] = balance;
        json["total_spent"] = total_spent;
        json["tier"] = static_cast<int>(tier);

        nlohmann::json coupon_array =
            nlohmann::json::array();

        for (const auto& coupon : coupons) {
            if (coupon) {
                coupon_array.push_back(coupon->toJson());
            }
        }

        nlohmann::json transaction_array =
            nlohmann::json::array();

        for (const auto& transaction : transactions) {
            if (transaction) {
                transaction_array.push_back(transaction->toJson());
            }
        }

        json["coupons"] = std::move(coupon_array);
        json["transactions"] = std::move(transaction_array);

        return json;
    }

    std::shared_ptr<CustomerAccount>
        CustomerAccount::fromJson(const nlohmann::json& json) {
        try {
            auto account = std::make_shared<CustomerAccount>(
                json.at("username").get<std::string>(),
                json.at("password_hash").get<std::string>(),
                json.at("created_at").get<std::string>(),
                json.value("balance", 0.0),
                json.value("total_spent", 0.0),
                parseTier(json.value("tier", 0))
            );

            if (const auto cpn_it = json.find("coupons");
                cpn_it != json.end() && cpn_it->is_array()) {
                for (const auto& item : *cpn_it) {
                    auto coupon = persist::CouponFactory::fromJson(item);
                    if (coupon) {
                        account->addCoupon(std::move(coupon));
                    }
                }
            }

            if (const auto tx_it = json.find("transactions");
                tx_it != json.end() && tx_it->is_array()) {
                for (const auto& item : *tx_it) {
                    auto transaction = persist::TransactionFactory::fromJson(item);
                    if (transaction) {
                        account->addTransaction(std::move(transaction));
                    }
                }
            }

            return account;
        }
        catch (const std::exception&) {
            return nullptr;
        }
    }

} // namespace user
