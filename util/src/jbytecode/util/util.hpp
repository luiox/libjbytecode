/// @defgroup util_util util 门面（Util）
///
/// util 模块的聚合辅助类。
#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace jbytecode::util {

class Util
{
public:
    template<typename T>
    static std::vector<T> asArrayList(const T* arr, size_t len)
    {
        if (arr == nullptr || len == 0) return {};
        return std::vector<T>(arr, arr + len);
    }

    static std::vector<std::string> asArrayList(size_t length)
    {
        return std::vector<std::string>(length);
    }
};

}   // namespace jbytecode::util
