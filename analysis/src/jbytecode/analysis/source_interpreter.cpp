#include "jbytecode/analysis/source_interpreter.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/tree/insn_list.hpp"

using namespace jbytecode::analysis;
using namespace jbytecode::core;
using namespace jbytecode::tree;

SourceValue SourceInterpreter::newValue(const Type* type)
{
    if (type == nullptr) return SourceValue(1);
    if (type->getSort() == Type::VOID) return SourceValue{};
    return SourceValue(type->getSize());
}

SourceValue SourceInterpreter::newOperation(const AbstractInsnNode* insn)
{
    int size;
    switch (insn->getOpcode()) {
    case Opcodes::LCONST_0:
    case Opcodes::LCONST_1:
    case Opcodes::DCONST_0:
    case Opcodes::DCONST_1: size = 2; break;
    case Opcodes::LDC:
    {
        auto* ldc = ca::core::tag_cast::cast<const LdcInsnNode*>(insn);
        if (ldc) {
            const auto& cst = ldc->cst;
            if (cst.is<ca::i64>() || cst.is<double>()) {
                size = 2;
            }
            else {
                size = 1;
            }
        }
        else {
            size = 1;
        }
        break;
    }
    case Opcodes::GETSTATIC:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) {
            size = Type::getType(field->desc).getSize();
        }
        else {
            size = 1;
        }
        break;
    }
    default: size = 1; break;
    }
    return SourceValue(size, insn);
}

SourceValue SourceInterpreter::copyOperation(const AbstractInsnNode* insn, const SourceValue& value)
{
    return SourceValue(value.getSize(), insn);
}

SourceValue SourceInterpreter::unaryOperation(const AbstractInsnNode* insn,
                                              const SourceValue&      value)
{
    (void)value;
    int size;
    switch (insn->getOpcode()) {
    case Opcodes::LNEG:
    case Opcodes::DNEG:
    case Opcodes::I2L:
    case Opcodes::I2D:
    case Opcodes::L2D:
    case Opcodes::F2L:
    case Opcodes::F2D:
    case Opcodes::D2L: size = 2; break;
    case Opcodes::GETFIELD:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) {
            size = Type::getType(field->desc).getSize();
        }
        else {
            size = 1;
        }
        break;
    }
    default: size = 1; break;
    }
    return SourceValue(size, insn);
}

SourceValue SourceInterpreter::binaryOperation(const AbstractInsnNode* insn,
                                               const SourceValue& value1, const SourceValue& value2)
{
    (void)value1;
    (void)value2;
    int size;
    switch (insn->getOpcode()) {
    case Opcodes::LALOAD:
    case Opcodes::DALOAD:
    case Opcodes::LADD:
    case Opcodes::DADD:
    case Opcodes::LSUB:
    case Opcodes::DSUB:
    case Opcodes::LMUL:
    case Opcodes::DMUL:
    case Opcodes::LDIV:
    case Opcodes::DDIV:
    case Opcodes::LREM:
    case Opcodes::DREM:
    case Opcodes::LSHL:
    case Opcodes::LSHR:
    case Opcodes::LUSHR:
    case Opcodes::LAND:
    case Opcodes::LOR:
    case Opcodes::LXOR: size = 2; break;
    default: size = 1; break;
    }
    return SourceValue(size, insn);
}

SourceValue SourceInterpreter::ternaryOperation(const AbstractInsnNode* insn,
                                                const SourceValue&      value1,
                                                const SourceValue&      value2,
                                                const SourceValue&      value3)
{
    (void)value1;
    (void)value2;
    (void)value3;
    return SourceValue(1, insn);
}

SourceValue SourceInterpreter::naryOperation(const AbstractInsnNode*         insn,
                                             const std::vector<SourceValue>& values)
{
    (void)values;
    int size = 1;
    if (insn->getOpcode() == Opcodes::MULTIANEWARRAY) {
        size = 1;
    }
    else if (insn->getOpcode() == Opcodes::INVOKEDYNAMIC) {
        auto* id = ca::core::tag_cast::cast<const InvokeDynamicInsnNode*>(insn);
        if (id) size = Type::getReturnType(id->desc).getSize();
    }
    else {
        auto* method = ca::core::tag_cast::cast<const MethodInsnNode*>(insn);
        if (method) size = Type::getReturnType(method->desc).getSize();
    }
    return SourceValue(size, insn);
}

void SourceInterpreter::returnOperation(const AbstractInsnNode* insn, const SourceValue& value,
                                        const SourceValue& expected)
{
    (void)insn;
    (void)value;
    (void)expected;
}

SourceValue SourceInterpreter::merge(const SourceValue& value1, const SourceValue& value2)
{
    auto setUnion = value1.insns_.union_(value2.insns_);
    if (setUnion == value1.insns_ && value1.size_ == value2.size_) {
        return value1;
    }
    else {
        return SourceValue(std::min(value1.size_, value2.size_), setUnion);
    }
}
