#include "jbytecode/analysis/basic_verifier.hpp"

#include <cstring>

using namespace jbytecode::analysis;
using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

// 字面量描述符 → Utf8StringRef 视图（字面量为静态存储，视图安全）。
ca::str::Utf8StringRef cstr(const char* s)
{
    return ca::str::Utf8StringRef::from_cstr(s);
}

}   // namespace

BasicValue BasicVerifier::copyOperation(const AbstractInsnNode* insn, const BasicValue& value)
{
    switch (insn->getOpcode()) {
    case Opcodes::ILOAD:
    case Opcodes::ISTORE:
        if (value != BasicValue::INT_VALUE)
            throw AnalyzerException(
                insn,
                "Expected " + BasicValue::INT_VALUE.toString() + ", but got " + value.toString());
        return value;
    case Opcodes::FLOAD:
    case Opcodes::FSTORE:
        if (value != BasicValue::FLOAT_VALUE)
            throw AnalyzerException(
                insn,
                "Expected " + BasicValue::FLOAT_VALUE.toString() + ", but got " + value.toString());
        return value;
    case Opcodes::LLOAD:
    case Opcodes::LSTORE:
        if (value != BasicValue::LONG_VALUE)
            throw AnalyzerException(
                insn,
                "Expected " + BasicValue::LONG_VALUE.toString() + ", but got " + value.toString());
        return value;
    case Opcodes::DLOAD:
    case Opcodes::DSTORE:
        if (value != BasicValue::DOUBLE_VALUE)
            throw AnalyzerException(insn,
                                    "Expected " + BasicValue::DOUBLE_VALUE.toString() +
                                        ", but got " + value.toString());
        return value;
    case Opcodes::ALOAD:
        if (!value.isReference())
            throw AnalyzerException(insn,
                                    "Expected an object reference, but got " + value.toString());
        return value;
    case Opcodes::ASTORE:
        if (!value.isReference() && value != BasicValue::RETURNADDRESS_VALUE)
            throw AnalyzerException(
                insn,
                "Expected an object reference or a return address, but got " + value.toString());
        return value;
    default: return value;
    }
}

BasicValue BasicVerifier::unaryOperation(const AbstractInsnNode* insn, const BasicValue& value)
{
    BasicValue expected;
    switch (insn->getOpcode()) {
    case Opcodes::INEG:
    case Opcodes::IINC:
    case Opcodes::I2F:
    case Opcodes::I2L:
    case Opcodes::I2D:
    case Opcodes::I2B:
    case Opcodes::I2C:
    case Opcodes::I2S:
    case Opcodes::IFEQ:
    case Opcodes::IFNE:
    case Opcodes::IFLT:
    case Opcodes::IFGE:
    case Opcodes::IFGT:
    case Opcodes::IFLE:
    case Opcodes::TABLESWITCH:
    case Opcodes::LOOKUPSWITCH:
    case Opcodes::IRETURN:
    case Opcodes::NEWARRAY:
    case Opcodes::ANEWARRAY: expected = BasicValue::INT_VALUE; break;
    case Opcodes::FNEG:
    case Opcodes::F2I:
    case Opcodes::F2L:
    case Opcodes::F2D:
    case Opcodes::FRETURN: expected = BasicValue::FLOAT_VALUE; break;
    case Opcodes::LNEG:
    case Opcodes::L2I:
    case Opcodes::L2F:
    case Opcodes::L2D:
    case Opcodes::LRETURN: expected = BasicValue::LONG_VALUE; break;
    case Opcodes::DNEG:
    case Opcodes::D2I:
    case Opcodes::D2F:
    case Opcodes::D2L:
    case Opcodes::DRETURN: expected = BasicValue::DOUBLE_VALUE; break;
    case Opcodes::GETFIELD:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) expected = newValueOf(Type::getObjectType(field->owner));
        break;
    }
    case Opcodes::ARRAYLENGTH:
        if (!isArrayValue(value))
            throw AnalyzerException(insn,
                                    "Expected an array reference, but got " + value.toString());
        return BasicInterpreter::unaryOperation(insn, value);
    case Opcodes::CHECKCAST:
    case Opcodes::ARETURN:
    case Opcodes::ATHROW:
    case Opcodes::INSTANCEOF:
    case Opcodes::MONITORENTER:
    case Opcodes::MONITOREXIT:
    case Opcodes::IFNULL:
    case Opcodes::IFNONNULL:
        if (!value.isReference())
            throw AnalyzerException(insn,
                                    "Expected an object reference, but got " + value.toString());
        return BasicInterpreter::unaryOperation(insn, value);
    case Opcodes::PUTSTATIC:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) expected = newValueOf(Type::getType(field->desc));
        break;
    }
    default: throw AnalyzerException(insn, "Unexpected opcode in unaryOperation");
    }
    if (!isSubTypeOf(value, expected))
        throw AnalyzerException(
            insn, "Expected " + expected.toString() + ", but got " + value.toString());
    return BasicInterpreter::unaryOperation(insn, value);
}

BasicValue BasicVerifier::binaryOperation(const AbstractInsnNode* insn, const BasicValue& value1,
                                          const BasicValue& value2)
{
    BasicValue expected1;
    BasicValue expected2;
    switch (insn->getOpcode()) {
    case Opcodes::IALOAD:
        expected1 = newValueOf(Type::getType(cstr("[I")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::BALOAD:
        if (isSubTypeOf(value1, newValueOf(Type::getType(cstr("[Z"))))) {
            expected1 = newValueOf(Type::getType(cstr("[Z")));
        }
        else {
            expected1 = newValueOf(Type::getType(cstr("[B")));
        }
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::CALOAD:
        expected1 = newValueOf(Type::getType(cstr("[C")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::SALOAD:
        expected1 = newValueOf(Type::getType(cstr("[S")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::LALOAD:
        expected1 = newValueOf(Type::getType(cstr("[J")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::FALOAD:
        expected1 = newValueOf(Type::getType(cstr("[F")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::DALOAD:
        expected1 = newValueOf(Type::getType(cstr("[D")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::AALOAD:
        expected1 = newValueOf(Type::getType(cstr("[Ljava/lang/Object;")));
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::IADD:
    case Opcodes::ISUB:
    case Opcodes::IMUL:
    case Opcodes::IDIV:
    case Opcodes::IREM:
    case Opcodes::ISHL:
    case Opcodes::ISHR:
    case Opcodes::IUSHR:
    case Opcodes::IAND:
    case Opcodes::IOR:
    case Opcodes::IXOR:
    case Opcodes::IF_ICMPEQ:
    case Opcodes::IF_ICMPNE:
    case Opcodes::IF_ICMPLT:
    case Opcodes::IF_ICMPGE:
    case Opcodes::IF_ICMPGT:
    case Opcodes::IF_ICMPLE:
        expected1 = BasicValue::INT_VALUE;
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::FADD:
    case Opcodes::FSUB:
    case Opcodes::FMUL:
    case Opcodes::FDIV:
    case Opcodes::FREM:
    case Opcodes::FCMPL:
    case Opcodes::FCMPG:
        expected1 = BasicValue::FLOAT_VALUE;
        expected2 = BasicValue::FLOAT_VALUE;
        break;
    case Opcodes::LADD:
    case Opcodes::LSUB:
    case Opcodes::LMUL:
    case Opcodes::LDIV:
    case Opcodes::LREM:
    case Opcodes::LAND:
    case Opcodes::LOR:
    case Opcodes::LXOR:
    case Opcodes::LCMP:
        expected1 = BasicValue::LONG_VALUE;
        expected2 = BasicValue::LONG_VALUE;
        break;
    case Opcodes::LSHL:
    case Opcodes::LSHR:
    case Opcodes::LUSHR:
        expected1 = BasicValue::LONG_VALUE;
        expected2 = BasicValue::INT_VALUE;
        break;
    case Opcodes::DADD:
    case Opcodes::DSUB:
    case Opcodes::DMUL:
    case Opcodes::DDIV:
    case Opcodes::DREM:
    case Opcodes::DCMPL:
    case Opcodes::DCMPG:
        expected1 = BasicValue::DOUBLE_VALUE;
        expected2 = BasicValue::DOUBLE_VALUE;
        break;
    case Opcodes::IF_ACMPEQ:
    case Opcodes::IF_ACMPNE:
        expected1 = BasicValue::REFERENCE_VALUE;
        expected2 = BasicValue::REFERENCE_VALUE;
        break;
    case Opcodes::PUTFIELD:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) {
            expected1 = newValueOf(Type::getObjectType(field->owner));
            expected2 = newValueOf(Type::getType(field->desc));
        }
        break;
    }
    default: throw AnalyzerException(insn, "Unexpected opcode in binaryOperation");
    }
    if (!isSubTypeOf(value1, expected1))
        throw AnalyzerException(
            insn,
            "Expected first argument " + expected1.toString() + ", but got " + value1.toString());
    if (!isSubTypeOf(value2, expected2))
        throw AnalyzerException(
            insn,
            "Expected second argument " + expected2.toString() + ", but got " + value2.toString());
    if (insn->getOpcode() == Opcodes::AALOAD) {
        return getElementValue(value1);
    }
    return BasicInterpreter::binaryOperation(insn, value1, value2);
}

BasicValue BasicVerifier::ternaryOperation(const AbstractInsnNode* insn, const BasicValue& value1,
                                           const BasicValue& value2, const BasicValue& value3)
{
    BasicValue expected1;
    BasicValue expected3;
    switch (insn->getOpcode()) {
    case Opcodes::IASTORE:
        expected1 = newValueOf(Type::getType(cstr("[I")));
        expected3 = BasicValue::INT_VALUE;
        break;
    case Opcodes::BASTORE:
        if (isSubTypeOf(value1, newValueOf(Type::getType(cstr("[Z"))))) {
            expected1 = newValueOf(Type::getType(cstr("[Z")));
        }
        else {
            expected1 = newValueOf(Type::getType(cstr("[B")));
        }
        expected3 = BasicValue::INT_VALUE;
        break;
    case Opcodes::CASTORE:
        expected1 = newValueOf(Type::getType(cstr("[C")));
        expected3 = BasicValue::INT_VALUE;
        break;
    case Opcodes::SASTORE:
        expected1 = newValueOf(Type::getType(cstr("[S")));
        expected3 = BasicValue::INT_VALUE;
        break;
    case Opcodes::LASTORE:
        expected1 = newValueOf(Type::getType(cstr("[J")));
        expected3 = BasicValue::LONG_VALUE;
        break;
    case Opcodes::FASTORE:
        expected1 = newValueOf(Type::getType(cstr("[F")));
        expected3 = BasicValue::FLOAT_VALUE;
        break;
    case Opcodes::DASTORE:
        expected1 = newValueOf(Type::getType(cstr("[D")));
        expected3 = BasicValue::DOUBLE_VALUE;
        break;
    case Opcodes::AASTORE:
        // 数组引用必须是引用类型数组（或 null）：用 [Ljava/lang/Object; 作期望类型，
        // SimpleVerifier 的 newValue 会保留数组类型做严格检查（int/String 等非数组
        // 引用被拒绝）；基础 BasicVerifier 模式退化为引用检查，与其它 *ASTORE 一致。
        // 修复前 expected1 = value1 是恒真自比，aastore 永不校验数组引用类型
        // （JVM 规范要求 VerifyError）。
        expected1 = newValueOf(Type::getType(cstr("[Ljava/lang/Object;")));
        expected3 = BasicValue::REFERENCE_VALUE;
        break;
    default: throw AnalyzerException(insn, "Unexpected opcode in ternaryOperation");
    }
    if (!isSubTypeOf(value1, expected1))
        throw AnalyzerException(insn,
                                "Expected first argument " + expected1.toString() +
                                    " array reference, but got " + value1.toString());
    if (value2 != BasicValue::INT_VALUE)
        throw AnalyzerException(insn,
                                "Expected second argument " + BasicValue::INT_VALUE.toString() +
                                    ", but got " + value2.toString());
    if (!isSubTypeOf(value3, expected3))
        throw AnalyzerException(
            insn,
            "Expected third argument " + expected3.toString() + ", but got " + value3.toString());
    return BasicValue{};
}

BasicValue BasicVerifier::naryOperation(const AbstractInsnNode*        insn,
                                        const std::vector<BasicValue>& values)
{
    int opcode = insn->getOpcode();
    if (opcode == Opcodes::MULTIANEWARRAY) {
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i] != BasicValue::INT_VALUE)
                throw AnalyzerException(insn,
                                        "Expected " + BasicValue::INT_VALUE.toString() +
                                            " for dimension " + std::to_string(i) + ", but got " +
                                            values[i].toString());
        }
    }
    else {
        size_t i = 0;
        size_t j = 0;
        if (opcode != Opcodes::INVOKESTATIC && opcode != Opcodes::INVOKEDYNAMIC) {
            auto* method = ca::core::tag_cast::cast<const MethodInsnNode*>(insn);
            if (method) {
                BasicValue ownerValue = newValueOf(Type::getObjectType(method->owner));
                if (!isSubTypeOf(values[i++], ownerValue))
                    throw AnalyzerException(insn,
                                            "Expected method owner " + ownerValue.toString() +
                                                ", but got " + values[0].toString());
            }
        }
        ca::str::Utf8StringRef methodDescriptor;
        if (opcode == Opcodes::INVOKEDYNAMIC) {
            auto* id = ca::core::tag_cast::cast<const InvokeDynamicInsnNode*>(insn);
            if (id) methodDescriptor = id->desc;
        }
        else {
            auto* method = ca::core::tag_cast::cast<const MethodInsnNode*>(insn);
            if (method) methodDescriptor = method->desc;
        }
        if (!methodDescriptor.is_empty()) {
            std::vector<Type> args = Type::getArgumentTypes(methodDescriptor);
            while (i < values.size()) {
                BasicValue expected = newValue(&args[j++]);
                BasicValue actual   = values[i++];
                if (!isSubTypeOf(actual, expected))
                    throw AnalyzerException(insn,
                                            "Expected argument " + std::to_string(j) + " " +
                                                expected.toString() + ", but got " +
                                                actual.toString());
            }
        }
    }
    return BasicInterpreter::naryOperation(insn, values);
}

void BasicVerifier::returnOperation(const AbstractInsnNode* insn, const BasicValue& value,
                                    const BasicValue& expected)
{
    if (!isSubTypeOf(value, expected))
        throw AnalyzerException(
            insn, "Expected return type " + expected.toString() + ", but got " + value.toString());
}

bool BasicVerifier::isArrayValue(const BasicValue& value)
{
    return value.isReference();
}

BasicValue BasicVerifier::getElementValue(const BasicValue& objectArrayValue)
{
    (void)objectArrayValue;
    return BasicValue::REFERENCE_VALUE;
}

bool BasicVerifier::isSubTypeOf(const BasicValue& value, const BasicValue& expected)
{
    return value.equals(expected);
}
