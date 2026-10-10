#include "method.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;

static const std::unordered_map<std::string, std::string>& getPrimitiveDescriptors()
{
    static const std::unordered_map<std::string, std::string> map = {{"void", "V"},
                                                                     {"byte", "B"},
                                                                     {"char", "C"},
                                                                     {"double", "D"},
                                                                     {"float", "F"},
                                                                     {"int", "I"},
                                                                     {"long", "J"},
                                                                     {"short", "S"},
                                                                     {"boolean", "Z"}};
    return map;
}

Method::Method(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
    : name_(name.data(), name.byte_length())
    , descriptor_(descriptor.data(), descriptor.byte_length())
{}

Method::Method(ca::str::Utf8StringRef name, const Type& returnType,
               const std::vector<Type>& argumentTypes)
    : name_(name.data(), name.byte_length())
{
    descriptor_ = Type::getMethodDescriptor(returnType, argumentTypes);
}

std::string Method::getDescriptorInternal(const std::string& type, bool defaultPackage)
{
    if (type.empty()) {
        return type;
    }

    std::string            buf;
    int                    arrayBracketsIndex = 0;
    std::string::size_type pos                = 0;
    while ((pos = type.find("[]", pos)) != std::string::npos) {
        buf.push_back('[');
        pos += 2;
        ++arrayBracketsIndex;
    }

    std::string elementType = type.substr(0, type.length() - arrayBracketsIndex * 2);
    const auto& primitives  = getPrimitiveDescriptors();
    auto        it          = primitives.find(elementType);
    if (it != primitives.end()) {
        buf += it->second;
    }
    else {
        buf.push_back('L');
        if (elementType.find('.') == std::string::npos && !defaultPackage) {
            buf += "java/lang/";
        }
        for (char c : elementType) {
            buf.push_back(c == '.' ? '/' : c);
        }
        buf.push_back(';');
    }
    return buf;
}

Method Method::getMethod(ca::str::Utf8StringRef method, bool defaultPackage)
{
    // 入口 Utf8StringRef 桥接到内部 std::string 解析（ASCII 方法声明文本处理）。
    std::string methodStr(reinterpret_cast<const char*>(method.data()), method.byte_length());
    auto        spaceIndex                = methodStr.find(' ');
    auto        currentArgumentStartIndex = methodStr.find('(', spaceIndex);
    if (currentArgumentStartIndex == std::string::npos) {
        throw std::invalid_argument("Invalid method declaration");
    }
    ++currentArgumentStartIndex;
    auto endIndex = methodStr.find(')', currentArgumentStartIndex);
    if (spaceIndex == std::string::npos || endIndex == std::string::npos) {
        throw std::invalid_argument("Invalid method declaration");
    }

    std::string returnType = methodStr.substr(0, spaceIndex);
    std::string methodName =
        methodStr.substr(spaceIndex + 1, currentArgumentStartIndex - spaceIndex - 2);

    auto nameStart = methodName.find_first_not_of(" \t");
    auto nameEnd   = methodName.find_last_not_of(" \t");
    if (nameStart != std::string::npos && nameEnd != std::string::npos) {
        methodName = methodName.substr(nameStart, nameEnd - nameStart + 1);
    }

    std::string desc;
    desc.push_back('(');
    while (currentArgumentStartIndex != std::string::npos) {
        auto        commaIndex = methodStr.find(',', currentArgumentStartIndex);
        std::string argType;
        if (commaIndex == std::string::npos || commaIndex > endIndex) {
            argType =
                methodStr.substr(currentArgumentStartIndex, endIndex - currentArgumentStartIndex);
            currentArgumentStartIndex = std::string::npos;
        }
        else {
            argType =
                methodStr.substr(currentArgumentStartIndex, commaIndex - currentArgumentStartIndex);
            currentArgumentStartIndex = commaIndex + 1;
        }
        auto trimStart = argType.find_first_not_of(" \t");
        auto trimEnd   = argType.find_last_not_of(" \t");
        if (trimStart != std::string::npos && trimEnd != std::string::npos) {
            argType = argType.substr(trimStart, trimEnd - trimStart + 1);
        }
        if (!argType.empty()) {
            desc += getDescriptorInternal(argType, defaultPackage);
        }
    }
    desc.push_back(')');
    desc += getDescriptorInternal(returnType, defaultPackage);

    return Method(ca::str::Utf8StringRef::from_cstr(methodName.c_str()),
                  ca::str::Utf8StringRef::from_cstr(desc.c_str()));
}
