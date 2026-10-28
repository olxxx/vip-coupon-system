#pragma once
#include <string>

namespace core
{

struct Result
{
    bool success;
    std::string message;

    static Result ok(std::string msg="操作成功")
    {
        return {true,std::move(msg)};
    }

    static Result fail(std::string error_msg)
    {
        return {false,std::move(error_msg)};
    }
};

} // namespace core
