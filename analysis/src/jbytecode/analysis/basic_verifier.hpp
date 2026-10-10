#pragma once

#include <vector>
#include <string>

#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "jbytecode/tree/insn_type_traits.hpp"
#include "jbytecode/analysis/basic_interpreter.hpp"

namespace jbytecode::analysis {

class BasicVerifier : public BasicInterpreter
{
public:
    BasicValue copyOperation(const jbytecode::tree::AbstractInsnNode* insn,
                             const BasicValue&                        value) override;

    BasicValue unaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                              const BasicValue&                        value) override;

    BasicValue binaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                               const BasicValue& value1, const BasicValue& value2) override;

    BasicValue ternaryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                                const BasicValue& value1, const BasicValue& value2,
                                const BasicValue& value3) override;

    BasicValue naryOperation(const jbytecode::tree::AbstractInsnNode* insn,
                             const std::vector<BasicValue>&           values) override;

    void returnOperation(const jbytecode::tree::AbstractInsnNode* insn, const BasicValue& value,
                         const BasicValue& expected) override;

protected:
    /// Type::getType/getObjectType 按值返回，取临时对象地址是 gcc 拒绝的
    /// address-of-rvalue（MSVC 宽容模式放行）；newValue 不持有指针，
    /// 经引用转发即可保语义等价。
    BasicValue newValueOf(const jbytecode::core::Type& type) { return newValue(&type); }
    virtual bool isArrayValue(const BasicValue& value);

    virtual BasicValue getElementValue(const BasicValue& objectArrayValue);

    virtual bool isSubTypeOf(const BasicValue& value, const BasicValue& expected);
};

}   // namespace jbytecode::analysis
