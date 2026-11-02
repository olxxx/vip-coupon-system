#pragma once

#include "common/Types.hpp"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace user {

    class Account {
    protected:
        std::string username;
        std::string password_hash;
        std::string created_at;

        nlohmann::json toJsonBase() const {
            return nlohmann::json{
                {"username", username},
                {"password_hash", password_hash},
                {"created_at", created_at}
            };
        }

    public:
        Account(std::string name,
            std::string password,
            std::string creation_time)
            : username(std::move(name)),
            password_hash(std::move(password)),
            created_at(std::move(creation_time)) {
        }

        virtual ~Account() = default;

        std::string getUsername() const {
            return username;
        }

        std::string getCreatedAt() const {
            return created_at;
        }

        std::string getPasswordHash() const {
            return password_hash;
        }

        bool verifyPassword(const std::string& input_password) const {
            return password_hash == input_password;
        }

        void setPassword(std::string new_password) {
            password_hash = std::move(new_password);
        }

        virtual core::AccountType getAccountType() const = 0;

        virtual std::string getDisplaySummary() const = 0;

        virtual nlohmann::json toJson() const = 0;
    };

} // namespace user