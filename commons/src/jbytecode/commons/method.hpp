#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include "jbytecode/core/type.hpp"
#include "libca/str/utf8_string.hpp"

namespace jbytecode::commons {

class Method
{
public:
    Method() = default;

    Method(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor);

    Method(ca::str::Utf8StringRef name, const jbytecode::core::Type& returnType,
           const std::vector<jbytecode::core::Type>& argumentTypes);

    static Method getMethod(ca::str::Utf8StringRef method) { return getMethod(method, false); }

    static Method getMethod(ca::str::Utf8StringRef method, bool defaultPackage);

    ca::str::Utf8StringRef getName() const { return name_.ref(); }
    ca::str::Utf8StringRef getDescriptor() const { return descriptor_.ref(); }

    jbytecode::core::Type getReturnType() const
    {
        return jbytecode::core::Type::getReturnType(descriptor_.ref());
    }

    std::vector<jbytecode::core::Type> getArgumentTypes() const
    {
        return jbytecode::core::Type::getArgumentTypes(descriptor_.ref());
    }

    ca::str::Utf8String toString() const
    {
        ca::str::Utf8StringBuilder b;
        b.append(name_);
        b.append(descriptor_);
        return b.build();
    }

    bool operator==(const Method& other) const
    {
        return name_ == other.name_ && descriptor_ == other.descriptor_;
    }

    bool operator!=(const Method& other) const { return !(*this == other); }

    struct Hash
    {
        size_t operator()(const Method& m) const
        {
            return std::hash<ca::str::Utf8String>()(m.name_) ^
                   std::hash<ca::str::Utf8String>()(m.descriptor_);
        }
    };

private:
    // 内部解析用 std::string（ASCII 方法声明文本处理）。
    static std::string getDescriptorInternal(const std::string& type, bool defaultPackage);

    ca::str::Utf8String name_;
    ca::str::Utf8String descriptor_;
};

}   // namespace jbytecode::commons
