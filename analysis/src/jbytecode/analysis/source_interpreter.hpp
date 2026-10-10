#pragma once

#include <algorithm>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"
#include "jbytecode/analysis/interpreter.hpp"
#include "jbytecode/analysis/small_set.hpp"

namespace jbytecode::analysis {
class SourceValue : public Value
{
public:
    int                                                size_;
    SmallSet<const jbytecode::tree::AbstractInsnNode*> insns_;

    SourceValue()
        : size_(0)
    {}

    explicit SourceValue(int size)
        : size_(size)
    {}

    SourceValue(int size, const jbytecode::tree::AbstractInsnNode* insn)
        : size_(size)
        , insns_(insn)
    {}

    SourceValue(int size, const SmallSet<const jbytecode::tree::AbstractInsnNode*>& insnSet)
        : size_(size)
        , insns_(insnSet)
    {}

    int getSize() const override { return size_; }

    bool operator==(const SourceValue& other) const
    {
        return size_ == other.size_ && insns_ == other.insns_;
    }

    bool operator!=(const SourceValue& other) const { return !(*this == other); }

    size_t hash() const
    {
        size_t h = static_cast<size_t>(size_);
        for (size_t i = 0; i < insns_.size(); i++) {
            h ^= reinterpret_cast<size_t>(insns_[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

class SourceInterpreter : public Interpreter<SourceValue>
{
public:
    SourceInterpreter() = default;

    SourceValue newValue(const jbytecode::core::Type* type) override;

    SourceValue newOperation(const jbytecode::tree::AbstractInsnNode* insn) override;

    SourceValue copyOperation(const jbytecode::tree::AbstractInsnNode* insn,
                              const SourceValue&                       value) override;

    SourceValue unaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                               const SourceValue&                       value) override;

    SourceValue binaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                                const SourceValue& value1, const SourceValue& value2) override;

    SourceValue ternaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                                 const SourceValue& value1, const SourceValue& value2,
                                 const SourceValue& value3) override;

    SourceValue naryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                              const std::vector<SourceValue>&          values) override;

    void returnOperation(const jbytecode::tree::AbstractInsnNode* insn, const SourceValue& value,
                         const SourceValue& expected) override;

    SourceValue merge(const SourceValue& value1, const SourceValue& value2) override;
};

}   // namespace jbytecode::analysis

namespace std {
template<>
struct hash<jbytecode::analysis::SourceValue>
{
    size_t operator()(const jbytecode::analysis::SourceValue& v) const { return v.hash(); }
};
}   // namespace std
