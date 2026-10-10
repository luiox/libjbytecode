#include "jbytecode/analysis/basic_interpreter.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include "jbytecode/tree/insn_list.hpp"
#include "libca/str/utf8_string.hpp"

#include <cstring>

using namespace jbytecode::analysis;
using namespace jbytecode::core;
using namespace jbytecode::tree;

using ca::str::Utf8StringRef;

const BasicValue BasicValue::UNINITIALIZED_VALUE(nullptr);
const BasicValue BasicValue::INT_VALUE(&Type::INT_TYPE);
const BasicValue BasicValue::FLOAT_VALUE(&Type::FLOAT_TYPE);
const BasicValue BasicValue::LONG_VALUE(&Type::LONG_TYPE);
const BasicValue BasicValue::DOUBLE_VALUE(&Type::DOUBLE_TYPE);
const Type       BasicValue::REFERENCE_TYPE =
    Type::getObjectType(Utf8StringRef::from_cstr("java/lang/Object"));
const BasicValue BasicValue::REFERENCE_VALUE(&REFERENCE_TYPE);
const BasicValue BasicValue::RETURNADDRESS_VALUE(&Type::VOID_TYPE);


const Type BasicInterpreter::NULL_TYPE = Type::getObjectType(Utf8StringRef::from_cstr("null"));

BasicValue BasicInterpreter::newValue(const Type* type)
{
    if (type == nullptr) return BasicValue::UNINITIALIZED_VALUE;
    switch (type->getSort()) {
    case Type::VOID: return BasicValue{};
    case Type::BOOLEAN:
    case Type::CHAR:
    case Type::BYTE:
    case Type::SHORT:
    case Type::INT: return BasicValue::INT_VALUE;
    case Type::FLOAT: return BasicValue::FLOAT_VALUE;
    case Type::LONG: return BasicValue::LONG_VALUE;
    case Type::DOUBLE: return BasicValue::DOUBLE_VALUE;
    case Type::ARRAY:
    case Type::OBJECT: return BasicValue::REFERENCE_VALUE;
    default: throw AnalyzerException("Unknown type sort");
    }
}

BasicValue BasicInterpreter::newOperation(const AbstractInsnNode* insn)
{
    switch (insn->getOpcode()) {
    case Opcodes::ACONST_NULL: return newValue(&NULL_TYPE);
    case Opcodes::ICONST_M1:
    case Opcodes::ICONST_0:
    case Opcodes::ICONST_1:
    case Opcodes::ICONST_2:
    case Opcodes::ICONST_3:
    case Opcodes::ICONST_4:
    case Opcodes::ICONST_5: return BasicValue::INT_VALUE;
    case Opcodes::LCONST_0:
    case Opcodes::LCONST_1: return BasicValue::LONG_VALUE;
    case Opcodes::FCONST_0:
    case Opcodes::FCONST_1:
    case Opcodes::FCONST_2: return BasicValue::FLOAT_VALUE;
    case Opcodes::DCONST_0:
    case Opcodes::DCONST_1: return BasicValue::DOUBLE_VALUE;
    case Opcodes::BIPUSH:
    case Opcodes::SIPUSH: return BasicValue::INT_VALUE;
    case Opcodes::LDC:
    {
        auto* ldc = ca::core::tag_cast::cast<const LdcInsnNode*>(insn);
        if (!ldc) break;
        const auto& cst = ldc->cst;
        if (cst.is<int>()) return BasicValue::INT_VALUE;
        if (cst.is<float>()) return BasicValue::FLOAT_VALUE;
        if (cst.is<ca::i64>()) return BasicValue::LONG_VALUE;
        if (cst.is<double>()) return BasicValue::DOUBLE_VALUE;
        if (cst.is<Utf8StringRef>())
            return newValue(Type::getObjectType(Utf8StringRef::from_cstr("java/lang/String")));
        if (cst.is<Type>())
            return newValue(Type::getObjectType(Utf8StringRef::from_cstr("java/lang/Class")));
        if (cst.is<Handle>())
            return newValue(
                Type::getObjectType(Utf8StringRef::from_cstr("java/lang/invoke/MethodHandle")));
        break;
    }
    case Opcodes::GETSTATIC:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) return newValue(Type::getType(field->desc));
        break;
    }
    case Opcodes::NEW:
    {
        auto* type = ca::core::tag_cast::cast<const TypeInsnNode*>(insn);
        if (type) return newValue(Type::getObjectType(type->desc));
        break;
    }
    default: break;
    }
    return BasicValue::UNINITIALIZED_VALUE;
}

BasicValue BasicInterpreter::copyOperation(const AbstractInsnNode* insn, const BasicValue& value)
{
    (void)insn;
    return value;
}

BasicValue BasicInterpreter::unaryOperation(const AbstractInsnNode* insn, const BasicValue& value)
{
    (void)value;
    switch (insn->getOpcode()) {
    case Opcodes::INEG:
    case Opcodes::IINC:
    case Opcodes::L2I:
    case Opcodes::F2I:
    case Opcodes::D2I:
    case Opcodes::I2B:
    case Opcodes::I2C:
    case Opcodes::I2S: return BasicValue::INT_VALUE;
    case Opcodes::FNEG:
    case Opcodes::I2F:
    case Opcodes::L2F:
    case Opcodes::D2F: return BasicValue::FLOAT_VALUE;
    case Opcodes::LNEG:
    case Opcodes::I2L:
    case Opcodes::F2L:
    case Opcodes::D2L: return BasicValue::LONG_VALUE;
    case Opcodes::DNEG:
    case Opcodes::I2D:
    case Opcodes::L2D:
    case Opcodes::F2D: return BasicValue::DOUBLE_VALUE;
    case Opcodes::IFEQ:
    case Opcodes::IFNE:
    case Opcodes::IFLT:
    case Opcodes::IFGE:
    case Opcodes::IFGT:
    case Opcodes::IFLE:
    case Opcodes::TABLESWITCH:
    case Opcodes::LOOKUPSWITCH:
    case Opcodes::IRETURN:
    case Opcodes::LRETURN:
    case Opcodes::FRETURN:
    case Opcodes::DRETURN:
    case Opcodes::ARETURN:
    case Opcodes::PUTSTATIC:
    case Opcodes::ATHROW:
    case Opcodes::MONITORENTER:
    case Opcodes::MONITOREXIT:
    case Opcodes::IFNULL:
    case Opcodes::IFNONNULL: return BasicValue{};
    case Opcodes::GETFIELD:
    {
        auto* field = ca::core::tag_cast::cast<const FieldInsnNode*>(insn);
        if (field) return newValue(Type::getType(field->desc));
        break;
    }
    case Opcodes::NEWARRAY:
    {
        auto* intInsn = ca::core::tag_cast::cast<const IntInsnNode*>(insn);
        if (!intInsn) break;
        switch (intInsn->operand) {
        case Opcodes::T_BOOLEAN: return newValue(Type::getType(Utf8StringRef::from_cstr("[Z")));
        case Opcodes::T_CHAR: return newValue(Type::getType(Utf8StringRef::from_cstr("[C")));
        case Opcodes::T_BYTE: return newValue(Type::getType(Utf8StringRef::from_cstr("[B")));
        case Opcodes::T_SHORT: return newValue(Type::getType(Utf8StringRef::from_cstr("[S")));
        case Opcodes::T_INT: return newValue(Type::getType(Utf8StringRef::from_cstr("[I")));
        case Opcodes::T_FLOAT: return newValue(Type::getType(Utf8StringRef::from_cstr("[F")));
        case Opcodes::T_DOUBLE: return newValue(Type::getType(Utf8StringRef::from_cstr("[D")));
        case Opcodes::T_LONG: return newValue(Type::getType(Utf8StringRef::from_cstr("[J")));
        default: throw AnalyzerException(insn, "Invalid array type");
        }
    }
    case Opcodes::ANEWARRAY:
    {
        auto* type = ca::core::tag_cast::cast<const TypeInsnNode*>(insn);
        if (type) {
            // 字符串线示范(总方案 §5.1):合成描述符优先 intern 进注入的作业 arena;
            // 无注入时回落实例私有 fallbackArena_(#921 staticArena 退役:兜底池从
            // 进程级降为解释器实例级,随解释器析构回收)。
            auto&                      arena   = arena_ ? *arena_ : fallbackArena_;
            auto                       objType = Type::getObjectType(type->desc);
            ca::str::Utf8StringBuilder fullDesc;
            fullDesc.append("[");
            fullDesc.append(objType.getDescriptor(arena));
            return newValue(Type::getType(arena.intern(fullDesc.build().ref())));
        }
        break;
    }
    case Opcodes::ARRAYLENGTH: return BasicValue::INT_VALUE;
    case Opcodes::CHECKCAST:
    case Opcodes::INSTANCEOF:
    {
        auto* type = ca::core::tag_cast::cast<const TypeInsnNode*>(insn);
        if (type) {
            if (insn->getOpcode() == Opcodes::CHECKCAST)
                return newValue(Type::getObjectType(type->desc));
            return BasicValue::INT_VALUE;
        }
        break;
    }
    default: break;
    }
    return BasicValue{};
}

BasicValue BasicInterpreter::binaryOperation(const AbstractInsnNode* insn, const BasicValue& value1,
                                             const BasicValue& value2)
{
    (void)value1;
    (void)value2;
    switch (insn->getOpcode()) {
    case Opcodes::IALOAD:
    case Opcodes::BALOAD:
    case Opcodes::CALOAD:
    case Opcodes::SALOAD:
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
    case Opcodes::LCMP:
    case Opcodes::FCMPL:
    case Opcodes::FCMPG:
    case Opcodes::DCMPL:
    case Opcodes::DCMPG: return BasicValue::INT_VALUE;
    case Opcodes::FALOAD:
    case Opcodes::FADD:
    case Opcodes::FSUB:
    case Opcodes::FMUL:
    case Opcodes::FDIV:
    case Opcodes::FREM: return BasicValue::FLOAT_VALUE;
    case Opcodes::LALOAD:
    case Opcodes::LADD:
    case Opcodes::LSUB:
    case Opcodes::LMUL:
    case Opcodes::LDIV:
    case Opcodes::LREM:
    case Opcodes::LAND:
    case Opcodes::LOR:
    case Opcodes::LXOR: return BasicValue::LONG_VALUE;
    case Opcodes::DALOAD:
    case Opcodes::DADD:
    case Opcodes::DSUB:
    case Opcodes::DMUL:
    case Opcodes::DDIV:
    case Opcodes::DREM: return BasicValue::DOUBLE_VALUE;
    case Opcodes::AALOAD: return BasicValue::REFERENCE_VALUE;
    case Opcodes::IF_ICMPEQ:
    case Opcodes::IF_ICMPNE:
    case Opcodes::IF_ICMPLT:
    case Opcodes::IF_ICMPGE:
    case Opcodes::IF_ICMPGT:
    case Opcodes::IF_ICMPLE:
    case Opcodes::IF_ACMPEQ:
    case Opcodes::IF_ACMPNE:
    case Opcodes::PUTFIELD: return BasicValue{};
    default: break;
    }
    return BasicValue{};
}

BasicValue BasicInterpreter::ternaryOperation(const AbstractInsnNode* insn,
                                              const BasicValue& value1, const BasicValue& value2,
                                              const BasicValue& value3)
{
    (void)insn;
    (void)value1;
    (void)value2;
    (void)value3;
    return BasicValue{};
}

BasicValue BasicInterpreter::naryOperation(const AbstractInsnNode*        insn,
                                           const std::vector<BasicValue>& values)
{
    (void)values;
    if (insn->getOpcode() == Opcodes::MULTIANEWARRAY) {
        auto* multi = ca::core::tag_cast::cast<const MultiANewArrayInsnNode*>(insn);
        if (multi) return newValue(Type::getType(multi->desc));
    }
    else if (insn->getOpcode() == Opcodes::INVOKEDYNAMIC) {
        auto* id = ca::core::tag_cast::cast<const InvokeDynamicInsnNode*>(insn);
        if (id) return newValue(Type::getReturnType(id->desc));
    }
    else {
        auto* method = ca::core::tag_cast::cast<const MethodInsnNode*>(insn);
        if (method) return newValue(Type::getReturnType(method->desc));
    }
    return BasicValue{};
}

void BasicInterpreter::returnOperation(const AbstractInsnNode* insn, const BasicValue& value,
                                       const BasicValue& expected)
{
    (void)insn;
    (void)value;
    (void)expected;
}

BasicValue BasicInterpreter::merge(const BasicValue& value1, const BasicValue& value2)
{
    if (!value1.equals(value2)) return BasicValue::UNINITIALIZED_VALUE;
    return value1;
}
