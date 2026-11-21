#pragma once

#include "common/Types.hpp"
#include "common/picosha2.h"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

using namespace std;

namespace user {

    class Account {
    protected:
        string username;
        string password_hash;
        string created_at;

        nlohmann::json toJsonBase() const {
            return nlohmann::json{
                {"username", username},
                {"password_hash", password_hash},
                {"created_at", created_at}
            };
        }//将C++对象的公共字段转化成JSON类型储存

    public:
        Account(string name,
            string password,
            string creation_time)
            : username(name),
            password_hash(password),
            created_at(creation_time) {
        }

        virtual ~Account() = default;//虚析构函数 保证用基类指针创造对象时，子类的析构函数也能正确运行

        static string hashPassword(const string& plain_password) {
            return picosha2::hash256_hex_string(plain_password);
        }/*把明文密码转换成SHA - 256哈希字符串
        static说明这是类函数，不需要Account对象就能调用，最后返回16进制字符串
        验证密码时直接比较password_hash == hashPassword(input_password)。哈希值无法直接还原，更加安全*/

        const string& getUsername() const {
            return username;
        }//第一个const：返回值不能被修改；第二个const：函数内不能修改username，所以可以被const对象调用

        const string& getCreatedAt() const {
            return created_at;
        }//读取账户创建时间

        const string& getPasswordHash() const {
            return password_hash;
        }//读取账户密码(哈希形式)

        bool verifyPassword(const string& input_password) const {
            return password_hash == hashPassword(input_password);
        }//验证密码(登录时)

        void setPassword(const string& new_password) {
            password_hash = hashPassword(new_password);
        }//设置或修改密码

        virtual core::AccountType getAccountType() const = 0;

        virtual string getDisplaySummary() const = 0;

        virtual nlohmann::json toJson() const = 0;
    };

} // namespace user 这样在user命名空间外写成user::Account的形式，便于辨认属于哪个模块


