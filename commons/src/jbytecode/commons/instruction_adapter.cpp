#include "jbytecode/commons/instruction_adapter.hpp"

#include <cstring>
#include <stdexcept>

#include "jbytecode/core/primitives.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

namespace {

ca::str::Utf8StringRef cstr(const char* s)
{
    return ca::str::Utf8StringRef(
        reinterpret_cast<const ca::u8*>(s), std::strlen(s), std::strlen(s));
}

}   // namespace

const Type InstructionAdapter::OBJECT_TYPE = Type::getObjectType(cstr("java/lang/Object"));

InstructionAdapter::InstructionAdapter(MethodVisitor* methodVisitor)
    : InstructionAdapter(Opcodes::ASM9, methodVisitor)
{}

InstructionAdapter::InstructionAdapter(int api, MethodVisitor* methodVisitor)
    : MethodVisitor(api, methodVisitor)
{}

// -- Visit method overrides --

void InstructionAdapter::visitInsn(int opcode)
{
    switch (opcode) {
    case Opcodes::NOP: nop(); break;
    case Opcodes::ACONST_NULL: aconst(ca::core::Any()); break;
    case Opcodes::ICONST_M1: iconst(-1); break;
    case Opcodes::ICONST_0: iconst(0); break;
    case Opcodes::ICONST_1: iconst(1); break;
    case Opcodes::ICONST_2: iconst(2); break;
    case Opcodes::ICONST_3: iconst(3); break;
    case Opcodes::ICONST_4: iconst(4); break;
    case Opcodes::ICONST_5: iconst(5); break;
    case Opcodes::LCONST_0: lconst(0); break;
    case Opcodes::LCONST_1: lconst(1); break;
    case Opcodes::FCONST_0: fconst(0.0f); break;
    case Opcodes::FCONST_1: fconst(1.0f); break;
    case Opcodes::FCONST_2: fconst(2.0f); break;
    case Opcodes::DCONST_0: dconst(0.0); break;
    case Opcodes::DCONST_1: dconst(1.0); break;
    case Opcodes::IALOAD: aload(Type::INT_TYPE); break;
    case Opcodes::LALOAD: aload(Type::LONG_TYPE); break;
    case Opcodes::FALOAD: aload(Type::FLOAT_TYPE); break;
    case Opcodes::DALOAD: aload(Type::DOUBLE_TYPE); break;
    case Opcodes::AALOAD: aload(OBJECT_TYPE); break;
    case Opcodes::BALOAD: aload(Type::BYTE_TYPE); break;
    case Opcodes::CALOAD: aload(Type::CHAR_TYPE); break;
    case Opcodes::SALOAD: aload(Type::SHORT_TYPE); break;
    case Opcodes::IASTORE: astore(Type::INT_TYPE); break;
    case Opcodes::LASTORE: astore(Type::LONG_TYPE); break;
    case Opcodes::FASTORE: astore(Type::FLOAT_TYPE); break;
    case Opcodes::DASTORE: astore(Type::DOUBLE_TYPE); break;
    case Opcodes::AASTORE: astore(OBJECT_TYPE); break;
    case Opcodes::BASTORE: astore(Type::BYTE_TYPE); break;
    case Opcodes::CASTORE: astore(Type::CHAR_TYPE); break;
    case Opcodes::SASTORE: astore(Type::SHORT_TYPE); break;
    case Opcodes::POP: pop(); break;
    case Opcodes::POP2: pop2(); break;
    case Opcodes::DUP: dup(); break;
    case Opcodes::DUP_X1: dupX1(); break;
    case Opcodes::DUP_X2: dupX2(); break;
    case Opcodes::DUP2: dup2(); break;
    case Opcodes::DUP2_X1: dup2X1(); break;
    case Opcodes::DUP2_X2: dup2X2(); break;
    case Opcodes::SWAP: swap(); break;
    case Opcodes::IADD: add(Type::INT_TYPE); break;
    case Opcodes::LADD: add(Type::LONG_TYPE); break;
    case Opcodes::FADD: add(Type::FLOAT_TYPE); break;
    case Opcodes::DADD: add(Type::DOUBLE_TYPE); break;
    case Opcodes::ISUB: sub(Type::INT_TYPE); break;
    case Opcodes::LSUB: sub(Type::LONG_TYPE); break;
    case Opcodes::FSUB: sub(Type::FLOAT_TYPE); break;
    case Opcodes::DSUB: sub(Type::DOUBLE_TYPE); break;
    case Opcodes::IMUL: mul(Type::INT_TYPE); break;
    case Opcodes::LMUL: mul(Type::LONG_TYPE); break;
    case Opcodes::FMUL: mul(Type::FLOAT_TYPE); break;
    case Opcodes::DMUL: mul(Type::DOUBLE_TYPE); break;
    case Opcodes::IDIV: div(Type::INT_TYPE); break;
    case Opcodes::LDIV: div(Type::LONG_TYPE); break;
    case Opcodes::FDIV: div(Type::FLOAT_TYPE); break;
    case Opcodes::DDIV: div(Type::DOUBLE_TYPE); break;
    case Opcodes::IREM: rem(Type::INT_TYPE); break;
    case Opcodes::LREM: rem(Type::LONG_TYPE); break;
    case Opcodes::FREM: rem(Type::FLOAT_TYPE); break;
    case Opcodes::DREM: rem(Type::DOUBLE_TYPE); break;
    case Opcodes::INEG: neg(Type::INT_TYPE); break;
    case Opcodes::LNEG: neg(Type::LONG_TYPE); break;
    case Opcodes::FNEG: neg(Type::FLOAT_TYPE); break;
    case Opcodes::DNEG: neg(Type::DOUBLE_TYPE); break;
    case Opcodes::ISHL: shl(Type::INT_TYPE); break;
    case Opcodes::LSHL: shl(Type::LONG_TYPE); break;
    case Opcodes::ISHR: shr(Type::INT_TYPE); break;
    case Opcodes::LSHR: shr(Type::LONG_TYPE); break;
    case Opcodes::IUSHR: ushr(Type::INT_TYPE); break;
    case Opcodes::LUSHR: ushr(Type::LONG_TYPE); break;
    case Opcodes::IAND: and_(Type::INT_TYPE); break;
    case Opcodes::LAND: and_(Type::LONG_TYPE); break;
    case Opcodes::IOR: or_(Type::INT_TYPE); break;
    case Opcodes::LOR: or_(Type::LONG_TYPE); break;
    case Opcodes::IXOR: xor_(Type::INT_TYPE); break;
    case Opcodes::LXOR: xor_(Type::LONG_TYPE); break;
    case Opcodes::I2L: cast(Type::INT_TYPE, Type::LONG_TYPE); break;
    case Opcodes::I2F: cast(Type::INT_TYPE, Type::FLOAT_TYPE); break;
    case Opcodes::I2D: cast(Type::INT_TYPE, Type::DOUBLE_TYPE); break;
    case Opcodes::L2I: cast(Type::LONG_TYPE, Type::INT_TYPE); break;
    case Opcodes::L2F: cast(Type::LONG_TYPE, Type::FLOAT_TYPE); break;
    case Opcodes::L2D: cast(Type::LONG_TYPE, Type::DOUBLE_TYPE); break;
    case Opcodes::F2I: cast(Type::FLOAT_TYPE, Type::INT_TYPE); break;
    case Opcodes::F2L: cast(Type::FLOAT_TYPE, Type::LONG_TYPE); break;
    case Opcodes::F2D: cast(Type::FLOAT_TYPE, Type::DOUBLE_TYPE); break;
    case Opcodes::D2I: cast(Type::DOUBLE_TYPE, Type::INT_TYPE); break;
    case Opcodes::D2L: cast(Type::DOUBLE_TYPE, Type::LONG_TYPE); break;
    case Opcodes::D2F: cast(Type::DOUBLE_TYPE, Type::FLOAT_TYPE); break;
    case Opcodes::I2B: cast(Type::INT_TYPE, Type::BYTE_TYPE); break;
    case Opcodes::I2C: cast(Type::INT_TYPE, Type::CHAR_TYPE); break;
    case Opcodes::I2S: cast(Type::INT_TYPE, Type::SHORT_TYPE); break;
    case Opcodes::LCMP: lcmp(); break;
    case Opcodes::FCMPL: cmpl(Type::FLOAT_TYPE); break;
    case Opcodes::FCMPG: cmpg(Type::FLOAT_TYPE); break;
    case Opcodes::DCMPL: cmpl(Type::DOUBLE_TYPE); break;
    case Opcodes::DCMPG: cmpg(Type::DOUBLE_TYPE); break;
    case Opcodes::IRETURN: areturn(Type::INT_TYPE); break;
    case Opcodes::LRETURN: areturn(Type::LONG_TYPE); break;
    case Opcodes::FRETURN: areturn(Type::FLOAT_TYPE); break;
    case Opcodes::DRETURN: areturn(Type::DOUBLE_TYPE); break;
    case Opcodes::ARETURN: areturn(OBJECT_TYPE); break;
    case Opcodes::RETURN: areturn(Type::VOID_TYPE); break;
    case Opcodes::ARRAYLENGTH: arraylength(); break;
    case Opcodes::ATHROW: athrow(); break;
    case Opcodes::MONITORENTER: monitorenter(); break;
    case Opcodes::MONITOREXIT: monitorexit(); break;
    default: throw std::invalid_argument("Illegal opcode in visitInsn");
    }
}

void InstructionAdapter::visitIntInsn(int opcode, int operand)
{
    switch (opcode) {
    case Opcodes::BIPUSH:
    case Opcodes::SIPUSH: iconst(operand); break;
    case Opcodes::NEWARRAY:
        switch (operand) {
        case Opcodes::T_BOOLEAN: newarray(Type::BOOLEAN_TYPE); break;
        case Opcodes::T_CHAR: newarray(Type::CHAR_TYPE); break;
        case Opcodes::T_BYTE: newarray(Type::BYTE_TYPE); break;
        case Opcodes::T_SHORT: newarray(Type::SHORT_TYPE); break;
        case Opcodes::T_INT: newarray(Type::INT_TYPE); break;
        case Opcodes::T_FLOAT: newarray(Type::FLOAT_TYPE); break;
        case Opcodes::T_LONG: newarray(Type::LONG_TYPE); break;
        case Opcodes::T_DOUBLE: newarray(Type::DOUBLE_TYPE); break;
        default: throw std::invalid_argument("Illegal array type in NEWARRAY");
        }
        break;
    default: throw std::invalid_argument("Illegal opcode in visitIntInsn");
    }
}

void InstructionAdapter::visitVarInsn(int opcode, int varIndex)
{
    switch (opcode) {
    case Opcodes::ILOAD: load(varIndex, Type::INT_TYPE); break;
    case Opcodes::LLOAD: load(varIndex, Type::LONG_TYPE); break;
    case Opcodes::FLOAD: load(varIndex, Type::FLOAT_TYPE); break;
    case Opcodes::DLOAD: load(varIndex, Type::DOUBLE_TYPE); break;
    case Opcodes::ALOAD: load(varIndex, OBJECT_TYPE); break;
    case Opcodes::ISTORE: store(varIndex, Type::INT_TYPE); break;
    case Opcodes::LSTORE: store(varIndex, Type::LONG_TYPE); break;
    case Opcodes::FSTORE: store(varIndex, Type::FLOAT_TYPE); break;
    case Opcodes::DSTORE: store(varIndex, Type::DOUBLE_TYPE); break;
    case Opcodes::ASTORE: store(varIndex, OBJECT_TYPE); break;
    case Opcodes::RET: ret(varIndex); break;
    default: throw std::invalid_argument("Illegal opcode in visitVarInsn");
    }
}

void InstructionAdapter::visitTypeInsn(int opcode, Utf8StringRef type)
{
    Type objectType = Type::getObjectType(type);
    switch (opcode) {
    case Opcodes::NEW: anew(objectType); break;
    case Opcodes::ANEWARRAY: newarray(objectType); break;
    case Opcodes::CHECKCAST: checkcast(objectType); break;
    case Opcodes::INSTANCEOF: instanceOf(objectType); break;
    default: throw std::invalid_argument("Illegal opcode in visitTypeInsn");
    }
}

void InstructionAdapter::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                        Utf8StringRef descriptor)
{
    switch (opcode) {
    case Opcodes::GETSTATIC: getstatic(owner, name, descriptor); break;
    case Opcodes::PUTSTATIC: putstatic(owner, name, descriptor); break;
    case Opcodes::GETFIELD: getfield(owner, name, descriptor); break;
    case Opcodes::PUTFIELD: putfield(owner, name, descriptor); break;
    default: throw std::invalid_argument("Illegal opcode in visitFieldInsn");
    }
}

void InstructionAdapter::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                         Utf8StringRef descriptor, bool isInterface)
{
    switch (opcode) {
    case Opcodes::INVOKESPECIAL: invokespecial(owner, name, descriptor, isInterface); break;
    case Opcodes::INVOKEVIRTUAL: invokevirtual(owner, name, descriptor, isInterface); break;
    case Opcodes::INVOKESTATIC: invokestatic(owner, name, descriptor, isInterface); break;
    case Opcodes::INVOKEINTERFACE: invokeinterface(owner, name, descriptor); break;
    default: throw std::invalid_argument("Illegal opcode in visitMethodInsn");
    }
}

void InstructionAdapter::visitInvokeDynamicInsn(
    Utf8StringRef name, Utf8StringRef descriptor, const Handle& bootstrapMethodHandle,
    const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    if (api < Opcodes::ASM5) {
        throw std::runtime_error("This feature requires ASM5");
    }
    invokedynamic(name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}

void InstructionAdapter::visitJumpInsn(int opcode, Label* label)
{
    switch (opcode) {
    case Opcodes::IFEQ: ifeq(label); break;
    case Opcodes::IFNE: ifne(label); break;
    case Opcodes::IFLT: iflt(label); break;
    case Opcodes::IFGE: ifge(label); break;
    case Opcodes::IFGT: ifgt(label); break;
    case Opcodes::IFLE: ifle(label); break;
    case Opcodes::IF_ICMPEQ: ificmpeq(label); break;
    case Opcodes::IF_ICMPNE: ificmpne(label); break;
    case Opcodes::IF_ICMPLT: ificmplt(label); break;
    case Opcodes::IF_ICMPGE: ificmpge(label); break;
    case Opcodes::IF_ICMPGT: ificmpgt(label); break;
    case Opcodes::IF_ICMPLE: ificmple(label); break;
    case Opcodes::IF_ACMPEQ: ifacmpeq(label); break;
    case Opcodes::IF_ACMPNE: ifacmpne(label); break;
    case Opcodes::GOTO: goTo(label); break;
    case Opcodes::JSR: jsr(label); break;
    case Opcodes::IFNULL: ifnull(label); break;
    case Opcodes::IFNONNULL: ifnonnull(label); break;
    default: throw std::invalid_argument("Illegal opcode in visitJumpInsn");
    }
}

void InstructionAdapter::visitLabel(Label* label)
{
    mark(label);
}

void InstructionAdapter::visitLdcInsn(const ca::core::Any& value)
{
    if (api < Opcodes::ASM5) {
        if (value.is<Handle>()) {
            throw std::runtime_error("This feature requires ASM5");
        }
        if (auto* t = value.as<Type>(); t && t->getSort() == Type::METHOD) {
            throw std::runtime_error("This feature requires ASM5");
        }
    }
    if (api < Opcodes::ASM7 && value.is<ConstantDynamic>()) {
        throw std::runtime_error("This feature requires ASM7");
    }

    if (value.is<int>()) {
        iconst(value.cast<int>());
    }
    else if (value.is<int64_t>()) {
        lconst(value.cast<int64_t>());
    }
    else if (value.is<float>()) {
        fconst(value.cast<float>());
    }
    else if (value.is<double>()) {
        dconst(value.cast<double>());
    }
    else if (value.is<Utf8StringRef>()) {
        aconst(value);
    }
    else if (value.is<Type>()) {
        tconst(value.cast<Type>());
    }
    else if (value.is<Handle>()) {
        hconst(value.cast<Handle>());
    }
    else if (value.is<ConstantDynamic>()) {
        cconst(value.cast<ConstantDynamic>());
    }
    else {
        throw std::invalid_argument("Illegal LDC value type");
    }
}

void InstructionAdapter::visitIincInsn(int varIndex, int increment)
{
    iinc(varIndex, increment);
}

void InstructionAdapter::visitTableSwitchInsn(int min, int max, Label* dflt,
                                              const std::vector<Label*>& labels)
{
    tableswitch(min, max, dflt, labels);
}

void InstructionAdapter::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                               const std::vector<Label*>& labels)
{
    lookupswitch(dflt, keys, labels);
}

void InstructionAdapter::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    multianewarray(descriptor, numDimensions);
}

// -- Named instruction methods --

void InstructionAdapter::nop()
{
    mv->visitInsn(Opcodes::NOP);
}

void InstructionAdapter::aconst(const ca::core::Any& value)
{
    if (!value.has_value()) {
        mv->visitInsn(Opcodes::ACONST_NULL);
    }
    else {
        mv->visitLdcInsn(value);
    }
}

void InstructionAdapter::iconst(int intValue)
{
    if (intValue >= -1 && intValue <= 5) {
        mv->visitInsn(Opcodes::ICONST_0 + intValue);
    }
    else if (intValue >= -128 && intValue <= 127) {
        mv->visitIntInsn(Opcodes::BIPUSH, intValue);
    }
    else if (intValue >= -32768 && intValue <= 32767) {
        mv->visitIntInsn(Opcodes::SIPUSH, intValue);
    }
    else {
        mv->visitLdcInsn(intValue);
    }
}

void InstructionAdapter::lconst(int64_t longValue)
{
    if (longValue == 0L || longValue == 1L) {
        mv->visitInsn(Opcodes::LCONST_0 + static_cast<int>(longValue));
    }
    else {
        mv->visitLdcInsn(longValue);
    }
}

void InstructionAdapter::fconst(float floatValue)
{
    int32_t bits;
    std::memcpy(&bits, &floatValue, sizeof(bits));
    if (bits == 0 || bits == 0x3F800000 || bits == 0x40000000) {
        mv->visitInsn(Opcodes::FCONST_0 + static_cast<int>(floatValue));
    }
    else {
        mv->visitLdcInsn(floatValue);
    }
}

void InstructionAdapter::dconst(double doubleValue)
{
    int64_t bits;
    std::memcpy(&bits, &doubleValue, sizeof(bits));
    if (bits == 0 || bits == 0x3FF0000000000000LL) {
        mv->visitInsn(Opcodes::DCONST_0 + static_cast<int>(doubleValue));
    }
    else {
        mv->visitLdcInsn(doubleValue);
    }
}

void InstructionAdapter::tconst(const Type& type)
{
    mv->visitLdcInsn(type);
}

void InstructionAdapter::hconst(const Handle& handle)
{
    mv->visitLdcInsn(handle);
}

void InstructionAdapter::cconst(const ConstantDynamic& constantDynamic)
{
    mv->visitLdcInsn(constantDynamic);
}

void InstructionAdapter::load(int varIndex, const Type& type)
{
    mv->visitVarInsn(type.getOpcode(Opcodes::ILOAD), varIndex);
}

void InstructionAdapter::aload(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IALOAD));
}

void InstructionAdapter::store(int varIndex, const Type& type)
{
    mv->visitVarInsn(type.getOpcode(Opcodes::ISTORE), varIndex);
}

void InstructionAdapter::astore(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IASTORE));
}

void InstructionAdapter::pop()
{
    mv->visitInsn(Opcodes::POP);
}

void InstructionAdapter::pop2()
{
    mv->visitInsn(Opcodes::POP2);
}

void InstructionAdapter::dup()
{
    mv->visitInsn(Opcodes::DUP);
}

void InstructionAdapter::dup2()
{
    mv->visitInsn(Opcodes::DUP2);
}

void InstructionAdapter::dupX1()
{
    mv->visitInsn(Opcodes::DUP_X1);
}

void InstructionAdapter::dupX2()
{
    mv->visitInsn(Opcodes::DUP_X2);
}

void InstructionAdapter::dup2X1()
{
    mv->visitInsn(Opcodes::DUP2_X1);
}

void InstructionAdapter::dup2X2()
{
    mv->visitInsn(Opcodes::DUP2_X2);
}

void InstructionAdapter::swap()
{
    mv->visitInsn(Opcodes::SWAP);
}

void InstructionAdapter::add(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IADD));
}

void InstructionAdapter::sub(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::ISUB));
}

void InstructionAdapter::mul(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IMUL));
}

void InstructionAdapter::div(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IDIV));
}

void InstructionAdapter::rem(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IREM));
}

void InstructionAdapter::neg(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::INEG));
}

void InstructionAdapter::shl(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::ISHL));
}

void InstructionAdapter::shr(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::ISHR));
}

void InstructionAdapter::ushr(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IUSHR));
}

void InstructionAdapter::and_(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IAND));
}

void InstructionAdapter::or_(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IOR));
}

void InstructionAdapter::xor_(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IXOR));
}

void InstructionAdapter::iinc(int varIndex, int increment)
{
    mv->visitIincInsn(varIndex, increment);
}

void InstructionAdapter::cast(const Type& from, const Type& to)
{
    cast(mv, from, to);
}

void InstructionAdapter::cast(MethodVisitor* methodVisitor, const Type& from, const Type& to)
{
    if (from != to) {
        if (from == Type::DOUBLE_TYPE) {
            if (to == Type::FLOAT_TYPE) {
                methodVisitor->visitInsn(Opcodes::D2F);
            }
            else if (to == Type::LONG_TYPE) {
                methodVisitor->visitInsn(Opcodes::D2L);
            }
            else {
                methodVisitor->visitInsn(Opcodes::D2I);
                cast(methodVisitor, Type::INT_TYPE, to);
            }
        }
        else if (from == Type::FLOAT_TYPE) {
            if (to == Type::DOUBLE_TYPE) {
                methodVisitor->visitInsn(Opcodes::F2D);
            }
            else if (to == Type::LONG_TYPE) {
                methodVisitor->visitInsn(Opcodes::F2L);
            }
            else {
                methodVisitor->visitInsn(Opcodes::F2I);
                cast(methodVisitor, Type::INT_TYPE, to);
            }
        }
        else if (from == Type::LONG_TYPE) {
            if (to == Type::DOUBLE_TYPE) {
                methodVisitor->visitInsn(Opcodes::L2D);
            }
            else if (to == Type::FLOAT_TYPE) {
                methodVisitor->visitInsn(Opcodes::L2F);
            }
            else {
                methodVisitor->visitInsn(Opcodes::L2I);
                cast(methodVisitor, Type::INT_TYPE, to);
            }
        }
        else {
            if (to == Type::BYTE_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2B);
            }
            else if (to == Type::CHAR_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2C);
            }
            else if (to == Type::DOUBLE_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2D);
            }
            else if (to == Type::FLOAT_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2F);
            }
            else if (to == Type::LONG_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2L);
            }
            else if (to == Type::SHORT_TYPE) {
                methodVisitor->visitInsn(Opcodes::I2S);
            }
        }
    }
}

void InstructionAdapter::lcmp()
{
    mv->visitInsn(Opcodes::LCMP);
}

void InstructionAdapter::cmpl(const Type& type)
{
    mv->visitInsn(type == Type::FLOAT_TYPE ? Opcodes::FCMPL : Opcodes::DCMPL);
}

void InstructionAdapter::cmpg(const Type& type)
{
    mv->visitInsn(type == Type::FLOAT_TYPE ? Opcodes::FCMPG : Opcodes::DCMPG);
}

void InstructionAdapter::ifeq(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFEQ, label);
}

void InstructionAdapter::ifne(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFNE, label);
}

void InstructionAdapter::iflt(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFLT, label);
}

void InstructionAdapter::ifge(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFGE, label);
}

void InstructionAdapter::ifgt(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFGT, label);
}

void InstructionAdapter::ifle(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFLE, label);
}

void InstructionAdapter::ificmpeq(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPEQ, label);
}

void InstructionAdapter::ificmpne(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPNE, label);
}

void InstructionAdapter::ificmplt(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPLT, label);
}

void InstructionAdapter::ificmpge(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPGE, label);
}

void InstructionAdapter::ificmpgt(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPGT, label);
}

void InstructionAdapter::ificmple(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ICMPLE, label);
}

void InstructionAdapter::ifacmpeq(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ACMPEQ, label);
}

void InstructionAdapter::ifacmpne(Label* label)
{
    mv->visitJumpInsn(Opcodes::IF_ACMPNE, label);
}

void InstructionAdapter::goTo(Label* label)
{
    mv->visitJumpInsn(Opcodes::GOTO, label);
}

void InstructionAdapter::jsr(Label* label)
{
    mv->visitJumpInsn(Opcodes::JSR, label);
}

void InstructionAdapter::ret(int varIndex)
{
    mv->visitVarInsn(Opcodes::RET, varIndex);
}

void InstructionAdapter::tableswitch(int min, int max, Label* dflt,
                                     const std::vector<Label*>& labels)
{
    mv->visitTableSwitchInsn(min, max, dflt, labels);
}

void InstructionAdapter::lookupswitch(Label* dflt, const std::vector<int>& keys,
                                      const std::vector<Label*>& labels)
{
    mv->visitLookupSwitchInsn(dflt, keys, labels);
}

void InstructionAdapter::areturn(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IRETURN));
}

void InstructionAdapter::getstatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                   ca::str::Utf8StringRef descriptor)
{
    mv->visitFieldInsn(Opcodes::GETSTATIC, owner, name, descriptor);
}

void InstructionAdapter::putstatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                   ca::str::Utf8StringRef descriptor)
{
    mv->visitFieldInsn(Opcodes::PUTSTATIC, owner, name, descriptor);
}

void InstructionAdapter::getfield(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                  ca::str::Utf8StringRef descriptor)
{
    mv->visitFieldInsn(Opcodes::GETFIELD, owner, name, descriptor);
}

void InstructionAdapter::putfield(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                  ca::str::Utf8StringRef descriptor)
{
    mv->visitFieldInsn(Opcodes::PUTFIELD, owner, name, descriptor);
}

void InstructionAdapter::invokevirtual(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor)
{
    if (api >= Opcodes::ASM5) {
        invokevirtual(owner, name, descriptor, false);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL, owner, name, descriptor, false);
}

void InstructionAdapter::invokevirtual(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor, bool isInterface)
{
    if (api < Opcodes::ASM5) {
        if (isInterface) {
            throw std::runtime_error("INVOKEVIRTUAL on interfaces requires ASM5");
        }
        invokevirtual(owner, name, descriptor);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL, owner, name, descriptor, isInterface);
}

void InstructionAdapter::invokespecial(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor)
{
    if (api >= Opcodes::ASM5) {
        invokespecial(owner, name, descriptor, false);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKESPECIAL, owner, name, descriptor, false);
}

void InstructionAdapter::invokespecial(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                       ca::str::Utf8StringRef descriptor, bool isInterface)
{
    if (api < Opcodes::ASM5) {
        if (isInterface) {
            throw std::runtime_error("INVOKESPECIAL on interfaces requires ASM5");
        }
        invokespecial(owner, name, descriptor);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKESPECIAL, owner, name, descriptor, isInterface);
}

void InstructionAdapter::invokestatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                      ca::str::Utf8StringRef descriptor)
{
    if (api >= Opcodes::ASM5) {
        invokestatic(owner, name, descriptor, false);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKESTATIC, owner, name, descriptor, false);
}

void InstructionAdapter::invokestatic(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                      ca::str::Utf8StringRef descriptor, bool isInterface)
{
    if (api < Opcodes::ASM5) {
        if (isInterface) {
            throw std::runtime_error("INVOKESTATIC on interfaces requires ASM5");
        }
        invokestatic(owner, name, descriptor);
        return;
    }
    mv->visitMethodInsn(Opcodes::INVOKESTATIC, owner, name, descriptor, isInterface);
}

void InstructionAdapter::invokeinterface(ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                                         ca::str::Utf8StringRef descriptor)
{
    mv->visitMethodInsn(Opcodes::INVOKEINTERFACE, owner, name, descriptor, true);
}

void InstructionAdapter::invokedynamic(ca::str::Utf8StringRef            name,
                                       ca::str::Utf8StringRef            descriptor,
                                       const Handle&                     bootstrapMethodHandle,
                                       const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    mv->visitInvokeDynamicInsn(name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}

void InstructionAdapter::anew(const Type& type)
{
    // #921 staticArena 退役：内部名瞬时传递，函数级局部 arena 零驻留。
    ca::str::Utf8StringArena scratch;
    mv->visitTypeInsn(Opcodes::NEW, type.getInternalName(scratch));
}

void InstructionAdapter::newarray(const Type& type)
{
    newarray(mv, type);
}

void InstructionAdapter::newarray(MethodVisitor* methodVisitor, const Type& type)
{
    switch (type.getSort()) {
    case Type::BOOLEAN: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_BOOLEAN); break;
    case Type::CHAR: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_CHAR); break;
    case Type::BYTE: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_BYTE); break;
    case Type::SHORT: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_SHORT); break;
    case Type::INT: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_INT); break;
    case Type::FLOAT: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_FLOAT); break;
    case Type::LONG: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_LONG); break;
    case Type::DOUBLE: methodVisitor->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_DOUBLE); break;
    default:
    {
        // #921 staticArena 退役：ARRAY 排序内部名会 intern，瞬时传递——函数级局部
        // arena 零驻留。
        ca::str::Utf8StringArena scratch;
        methodVisitor->visitTypeInsn(Opcodes::ANEWARRAY, type.getInternalName(scratch));
        break;
    }
    }
}

void InstructionAdapter::arraylength()
{
    mv->visitInsn(Opcodes::ARRAYLENGTH);
}

void InstructionAdapter::athrow()
{
    mv->visitInsn(Opcodes::ATHROW);
}

void InstructionAdapter::checkcast(const Type& type)
{
    ca::str::Utf8StringArena scratch;   // #921 staticArena 退役：同 anew，瞬时零驻留。
    mv->visitTypeInsn(Opcodes::CHECKCAST, type.getInternalName(scratch));
}

void InstructionAdapter::instanceOf(const Type& type)
{
    ca::str::Utf8StringArena scratch;   // #921 staticArena 退役：同 anew，瞬时零驻留。
    mv->visitTypeInsn(Opcodes::INSTANCEOF, type.getInternalName(scratch));
}

void InstructionAdapter::monitorenter()
{
    mv->visitInsn(Opcodes::MONITORENTER);
}

void InstructionAdapter::monitorexit()
{
    mv->visitInsn(Opcodes::MONITOREXIT);
}

void InstructionAdapter::multianewarray(ca::str::Utf8StringRef descriptor, int numDimensions)
{
    mv->visitMultiANewArrayInsn(descriptor, numDimensions);
}

void InstructionAdapter::ifnull(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFNULL, label);
}

void InstructionAdapter::ifnonnull(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFNONNULL, label);
}

void InstructionAdapter::mark(Label* label)
{
    mv->visitLabel(label);
}
