/// @defgroup util_asmifier_support ASMifier 支撑
///
/// ASMifier 的内部辅助（文本拼装等）。
#pragma once

#include <string>
#include <unordered_map>

#include "jbytecode/core/primitives.hpp"

namespace jbytecode::util {

// Interface for custom Attribute implementations that can generate
// ASMifier Java source code (mirrors Java ASMifierSupport).
class ASMifierSupport
{
public:
    virtual ~ASMifierSupport() = default;
    virtual void asmify(
        std::string& output, const std::string& visitorVariableName,
        const std::unordered_map<jbytecode::core::Label*, std::string>& labelNames) = 0;
};

}   // namespace jbytecode::util
