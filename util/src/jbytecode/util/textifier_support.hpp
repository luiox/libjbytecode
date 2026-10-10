#pragma once

#include <string>
#include <unordered_map>

#include "jbytecode/core/primitives.hpp"

namespace jbytecode::util {

// Interface for custom Attribute implementations that can generate
// human-readable text (mirrors Java TextifierSupport).
class TextifierSupport
{
public:
    virtual ~TextifierSupport() = default;
    virtual void textify(
        std::string&                                                    output,
        const std::unordered_map<jbytecode::core::Label*, std::string>& labelNames) = 0;
};

}   // namespace jbytecode::util
