#include "jbytecode/commons/analyzer_adapter.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/primitives.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>

using namespace jbytecode::commons;
using namespace jbytecode::core;
using namespace ca::str;

namespace {

bool isInt(const ca::core::Any& value)
{
    return value.as<int>() != nullptr;
}

bool isIntEq(const ca::core::Any& value, int c)
{
    auto* p = value.as<int>();
    return p && *p == c;
}

bool isString(const ca::core::Any& value)
{
    return value.as<Utf8StringRef>() != nullptr;
}

bool isLabel(const ca::core::Any& value)
{
    return value.as<Label*>() != nullptr;
}

bool anyRefEq(const ca::core::Any& a, const ca::core::Any& b)
{
    if (auto* ip = a.as<int>()) {
        auto* jp = b.as<int>();
        return jp && *ip == *jp;
    }
    if (auto* sp = a.as<Utf8StringRef>()) {
        auto* tp = b.as<Utf8StringRef>();
        return tp && *sp == *tp;
    }
    if (auto* lp = a.as<Label*>()) {
        auto* mp = b.as<Label*>();
        return mp && *lp == *mp;
    }
    return false;
}

}   // anonymous namespace

AnalyzerAdapter::AnalyzerAdapter(Utf8StringRef owner, int access, Utf8StringRef name,
                                 Utf8StringRef descriptor, MethodVisitor* mv,
                                 ca::str::Utf8StringArena* arena)
    : MethodVisitor(Opcodes::ASM9, mv)
    , owner_(owner)
    , arena_(arena)
{

    if ((access & Opcodes::ACC_STATIC) == 0) {
        if (name == "<init>") {
            locals_.push_back(Opcodes::UNINITIALIZED_THIS);
        }
        else {
            locals_.push_back(owner_);
        }
    }

    auto argTypes = Type::getArgumentTypes(descriptor);
    for (const auto& argType : argTypes) {
        // #921 staticArena 退役：ARRAY 描述符 intern 落点=注入作业 arena 或实例私有
        // descArena_（与 execute() 的 ANEWARRAY 同口径），随 adapter 析构回收。
        ca::str::Utf8StringArena& pool = arena_ ? *arena_ : descArena_;
        switch (argType.getSort()) {
        case Type::BOOLEAN:
        case Type::CHAR:
        case Type::BYTE:
        case Type::SHORT:
        case Type::INT: locals_.push_back(Opcodes::INTEGER); break;
        case Type::FLOAT: locals_.push_back(Opcodes::FLOAT); break;
        case Type::LONG:
            locals_.push_back(Opcodes::LONG);
            locals_.push_back(Opcodes::TOP);
            break;
        case Type::DOUBLE:
            locals_.push_back(Opcodes::DOUBLE);
            locals_.push_back(Opcodes::TOP);
            break;
        case Type::ARRAY: locals_.push_back(argType.getDescriptor(pool)); break;
        case Type::OBJECT: locals_.push_back(argType.getInternalName(pool)); break;
        default: throw std::logic_error("Unexpected type sort in constructor");
        }
    }
    maxLocals_ = static_cast<int>(locals_.size());
}

void AnalyzerAdapter::visitFrameTypes(int numTypes, const std::vector<ca::core::Any>& frameTypes,
                                      std::vector<ca::core::Any>& result)
{
    for (int i = 0; i < numTypes; ++i) {
        const auto& frameType = frameTypes[i];
        result.push_back(frameType);
        if (isIntEq(frameType, Opcodes::LONG) || isIntEq(frameType, Opcodes::DOUBLE)) {
            result.push_back(Opcodes::TOP);
        }
    }
}

void AnalyzerAdapter::visitFrame(int type, int numLocal, const std::vector<ca::core::Any>& local,
                                 int numStack, const std::vector<ca::core::Any>& stack)
{
    if (type != Opcodes::F_NEW) {
        throw std::invalid_argument(
            "AnalyzerAdapter only accepts expanded frames (see ClassReader.EXPAND_FRAMES)");
    }

    MethodVisitor::visitFrame(type, numLocal, local, numStack, stack);

    if (!frameDead_) {
        labels_.clear();
        return;
    }

    // 通过 NEW frame 重新激活
    locals_.clear();
    stack_.clear();
    frameDead_ = false;
    visitFrameTypes(numLocal, local, locals_);
    visitFrameTypes(numStack, stack, stack_);
}

void AnalyzerAdapter::visitInsn(int opcode)
{
    MethodVisitor::visitInsn(opcode);
    execute(opcode, 0, Utf8StringRef());
}

void AnalyzerAdapter::visitIntInsn(int opcode, int operand)
{
    MethodVisitor::visitIntInsn(opcode, operand);
    execute(opcode, operand, Utf8StringRef());
}

void AnalyzerAdapter::visitVarInsn(int opcode, int varIndex)
{
    if (frameDead_) {
        labels_.clear();
        return;
    }
    bool isLongOrDouble = opcode == Opcodes::LLOAD || opcode == Opcodes::DLOAD ||
                          opcode == Opcodes::LSTORE || opcode == Opcodes::DSTORE;
    maxLocals_ = std::max(maxLocals_, varIndex + (isLongOrDouble ? 2 : 1));
    execute(opcode, varIndex, Utf8StringRef());
}

void AnalyzerAdapter::visitTypeInsn(int opcode, Utf8StringRef type)
{
    if (opcode == Opcodes::NEW) {
        if (labels_.empty()) {
            auto* label = labelArena_.create<Label>();
            labels_.push_back(label);
            if (mv) mv->visitLabel(label);
        }
        for (auto* label : labels_) {
            uninitializedTypes_.insert_or_assign(
                label, ca::str::Utf8String(type.data(), type.byte_length()));
        }
    }
    MethodVisitor::visitTypeInsn(opcode, type);
    execute(opcode, 0, type);
}

void AnalyzerAdapter::visitFieldInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                     Utf8StringRef descriptor)
{
    MethodVisitor::visitFieldInsn(opcode, owner, name, descriptor);
    execute(opcode, 0, descriptor);
}

void AnalyzerAdapter::visitMethodInsn(int opcode, Utf8StringRef owner, Utf8StringRef name,
                                      Utf8StringRef descriptor, bool isInterface)
{
    MethodVisitor::visitMethodInsn(opcode, owner, name, descriptor, isInterface);

    if (frameDead_) {
        labels_.clear();
        return;
    }

    pop(descriptor);
    if (opcode != Opcodes::INVOKESTATIC) {
        ca::core::Any value = pop();
        if (opcode == Opcodes::INVOKESPECIAL && name == "<init>") {
            ca::core::Any initializedValue;
            if (isIntEq(value, Opcodes::UNINITIALIZED_THIS)) {
                initializedValue = owner_;
            }
            else {
                initializedValue = owner;
            }
            for (size_t i = 0; i < locals_.size(); ++i) {
                if (anyRefEq(locals_[i], value)) {
                    locals_[i] = initializedValue;
                }
            }
            for (size_t i = 0; i < stack_.size(); ++i) {
                if (anyRefEq(stack_[i], value)) {
                    stack_[i] = initializedValue;
                }
            }
        }
    }

    // 推入返回值
    if (!descriptor.is_empty() && descriptor.byte_at(0) == '(') {
        // #921 staticArena 退役：返回类型 METHOD 排序，getDescriptor 返回 value_ 不 intern，
        // 落点仍按统一口径走注入 arena/实例池。
        pushDescriptor(Type::getReturnType(descriptor).getDescriptor(arena_ ? *arena_ : descArena_));
    }
    else {
        pushDescriptor(descriptor);
    }

    labels_.clear();
}

void AnalyzerAdapter::visitInvokeDynamicInsn(
    Utf8StringRef name, Utf8StringRef descriptor, const Handle& bootstrapMethodHandle,
    const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    MethodVisitor::visitInvokeDynamicInsn(
        name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
    execute(Opcodes::INVOKEDYNAMIC, 0, descriptor);
}

void AnalyzerAdapter::visitJumpInsn(int opcode, Label* label)
{
    MethodVisitor::visitJumpInsn(opcode, label);
    execute(opcode, 0, Utf8StringRef());
}

void AnalyzerAdapter::visitLabel(Label* label)
{
    MethodVisitor::visitLabel(label);
    frameDead_ = false;
    labels_.push_back(label);
}

void AnalyzerAdapter::visitLdcInsn(const ca::core::Any& value)
{
    MethodVisitor::visitLdcInsn(value);
    labels_.clear();

    if (value.is<int>()) {
        push(Opcodes::INTEGER);
    }
    else if (value.is<ca::i64>()) {
        push(Opcodes::LONG);
        push(Opcodes::TOP);
    }
    else if (value.is<float>()) {
        push(Opcodes::FLOAT);
    }
    else if (value.is<double>()) {
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
    }
    else if (value.is<Utf8StringRef>()) {
        push(Utf8StringRef::from_cstr("java/lang/String"));
    }
    else if (value.is<Type>()) {
        Type t    = value.cast<Type>();
        int  sort = t.getSort();
        if (sort == Type::OBJECT || sort == Type::ARRAY) {
            push(Utf8StringRef::from_cstr("java/lang/Class"));
        }
        else if (sort == Type::METHOD) {
            push(Utf8StringRef::from_cstr("java/lang/invoke/MethodType"));
        }
        else {
            throw std::invalid_argument("Illegal LDC type sort");
        }
    }
    else if (value.is<Handle>()) {
        push(Utf8StringRef::from_cstr("java/lang/invoke/MethodHandle"));
    }
    else {
        throw std::invalid_argument("Illegal LDC value type");
    }
    labels_.clear();
}

void AnalyzerAdapter::visitIincInsn(int varIndex, int increment)
{
    MethodVisitor::visitIincInsn(varIndex, increment);
    maxLocals_ = std::max(maxLocals_, varIndex + 1);
    execute(Opcodes::IINC, varIndex, Utf8StringRef());
}

void AnalyzerAdapter::visitTableSwitchInsn(int min, int max, Label* dflt,
                                           const std::vector<Label*>& labels)
{
    MethodVisitor::visitTableSwitchInsn(min, max, dflt, labels);
    execute(Opcodes::TABLESWITCH, 0, Utf8StringRef());
    locals_.clear();
    stack_.clear();
    frameDead_ = true;
}

void AnalyzerAdapter::visitLookupSwitchInsn(Label* dflt, const std::vector<int>& keys,
                                            const std::vector<Label*>& labels)
{
    MethodVisitor::visitLookupSwitchInsn(dflt, keys, labels);
    execute(Opcodes::LOOKUPSWITCH, 0, Utf8StringRef());
    locals_.clear();
    stack_.clear();
    frameDead_ = true;
}

void AnalyzerAdapter::visitMultiANewArrayInsn(Utf8StringRef descriptor, int numDimensions)
{
    MethodVisitor::visitMultiANewArrayInsn(descriptor, numDimensions);
    execute(Opcodes::MULTIANEWARRAY, numDimensions, descriptor);
}

void AnalyzerAdapter::visitLocalVariable(Utf8StringRef name, Utf8StringRef descriptor,
                                         Utf8StringRef signature, Label* start, Label* end,
                                         int index)
{
    maxLocals_ = std::max(
        maxLocals_, index + (descriptor.byte_at(0) == 'J' || descriptor.byte_at(0) == 'D' ? 2 : 1));
    MethodVisitor::visitLocalVariable(name, descriptor, signature, start, end, index);
}

void AnalyzerAdapter::visitMaxs(int maxStack, int maxLocals)
{
    if (mv) {
        maxStack_  = std::max(maxStack_, maxStack);
        maxLocals_ = std::max(maxLocals_, maxLocals);
        mv->visitMaxs(maxStack_, maxLocals_);
    }
}

// -----------------------------------------------------------------------
// Frame simulation helpers
// -----------------------------------------------------------------------

ca::core::Any AnalyzerAdapter::get(int local)
{
    maxLocals_ = std::max(maxLocals_, local + 1);
    return local < static_cast<int>(locals_.size()) ? locals_[local] : ca::core::Any(Opcodes::TOP);
}

void AnalyzerAdapter::set(int local, const ca::core::Any& type)
{
    maxLocals_ = std::max(maxLocals_, local + 1);
    while (local >= static_cast<int>(locals_.size())) {
        locals_.push_back(Opcodes::TOP);
    }
    locals_[local] = type;
}

void AnalyzerAdapter::push(const ca::core::Any& type)
{
    stack_.push_back(type);
    maxStack_ = std::max(maxStack_, static_cast<int>(stack_.size()));
}

void AnalyzerAdapter::pushDescriptor(Utf8StringRef fieldOrMethodDescriptor)
{
    // #921 staticArena 退役：METHOD 排序 getDescriptor 返回 value_ 不 intern，落点仍走
    // 注入 arena/实例池（与 execute() 合成描述符同口径）。
    Utf8StringRef desc = (fieldOrMethodDescriptor.byte_at(0) == '(')
                             ? Type::getReturnType(fieldOrMethodDescriptor)
                                   .getDescriptor(arena_ ? *arena_ : descArena_)
                             : fieldOrMethodDescriptor;
    if (desc.is_empty()) return;
    switch (desc.byte_at(0)) {
    case 'V': return;
    case 'Z':
    case 'C':
    case 'B':
    case 'S':
    case 'I': push(Opcodes::INTEGER); return;
    case 'F': push(Opcodes::FLOAT); return;
    case 'J':
        push(Opcodes::LONG);
        push(Opcodes::TOP);
        return;
    case 'D':
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
        return;
    case '[': push(desc); break;
    case 'L': push(desc.slice(1, desc.byte_length() - 1)); break;
    default: throw std::logic_error("Unexpected descriptor char");
    }
}

ca::core::Any AnalyzerAdapter::pop()
{
    ca::core::Any val = std::move(stack_.back());
    stack_.pop_back();
    return val;
}

void AnalyzerAdapter::pop(int numSlots)
{
    for (int i = 0; i < numSlots; ++i) {
        stack_.pop_back();
    }
}

void AnalyzerAdapter::pop(Utf8StringRef descriptor)
{
    char firstChar = static_cast<char>(descriptor.byte_at(0));
    if (firstChar == '(') {
        int  numSlots = 0;
        auto types    = Type::getArgumentTypes(descriptor);
        for (const auto& type : types) {
            numSlots += type.getSize();
        }
        pop(numSlots);
    }
    else if (firstChar == 'J' || firstChar == 'D') {
        pop(2);
    }
    else {
        pop(1);
    }
}

void AnalyzerAdapter::execute(int opcode, int intArg, Utf8StringRef stringArg)
{
    if (opcode == Opcodes::JSR || opcode == Opcodes::RET) {
        throw std::invalid_argument("JSR/RET are not supported");
    }
    if (frameDead_) {
        labels_.clear();
        return;
    }

    ca::core::Any value1;
    ca::core::Any value2;
    ca::core::Any value3;
    ca::core::Any t4;

    switch (opcode) {
    case Opcodes::NOP:
    case Opcodes::INEG:
    case Opcodes::LNEG:
    case Opcodes::FNEG:
    case Opcodes::DNEG:
    case Opcodes::I2B:
    case Opcodes::I2C:
    case Opcodes::I2S:
    case Opcodes::GOTO:
    case Opcodes::RETURN: break;

    case Opcodes::ACONST_NULL: push(Opcodes::ITEM_NULL); break;

    case Opcodes::ICONST_M1:
    case Opcodes::ICONST_0:
    case Opcodes::ICONST_1:
    case Opcodes::ICONST_2:
    case Opcodes::ICONST_3:
    case Opcodes::ICONST_4:
    case Opcodes::ICONST_5: push(Opcodes::INTEGER); break;

    case Opcodes::LCONST_0:
    case Opcodes::LCONST_1:
        push(Opcodes::LONG);
        push(Opcodes::TOP);
        break;

    case Opcodes::FCONST_0:
    case Opcodes::FCONST_1:
    case Opcodes::FCONST_2: push(Opcodes::FLOAT); break;

    case Opcodes::DCONST_0:
    case Opcodes::DCONST_1:
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
        break;

    case Opcodes::BIPUSH:
    case Opcodes::SIPUSH: push(Opcodes::INTEGER); break;

    case Opcodes::LDC: break;

    case Opcodes::ILOAD:
    case Opcodes::FLOAD:
    case Opcodes::ALOAD:
    case Opcodes::LLOAD:
    case Opcodes::DLOAD:
    case Opcodes::ISTORE:
    case Opcodes::FSTORE:
    case Opcodes::ASTORE:
    case Opcodes::LSTORE:
    case Opcodes::DSTORE:
    case Opcodes::RET: break;

    case Opcodes::IALOAD:
    case Opcodes::BALOAD:
    case Opcodes::CALOAD:
    case Opcodes::SALOAD:
        pop(2);
        push(Opcodes::INTEGER);
        break;

    case Opcodes::LALOAD:
    case Opcodes::DALOAD:
    case Opcodes::L2D:
        pop(2);
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
        break;

    case Opcodes::FALOAD:
    case Opcodes::I2F:
    case Opcodes::L2F:
    case Opcodes::D2F:
        pop(1);
        push(Opcodes::FLOAT);
        break;

    case Opcodes::AALOAD:
        pop(1);
        value1 = pop();
        if (auto* sp = value1.as<Utf8StringRef>()) {
            pushDescriptor(sp->slice(1, sp->byte_length()));
        }
        else if (isIntEq(value1, Opcodes::ITEM_NULL)) {
            push(value1);
        }
        else {
            push(Utf8StringRef::from_cstr("java/lang/Object"));
        }
        break;

    case Opcodes::AASTORE: pop(3); break;

    case Opcodes::BASTORE:
    case Opcodes::CASTORE:
    case Opcodes::SASTORE:
    case Opcodes::IASTORE:
    case Opcodes::FASTORE: pop(3); break;

    case Opcodes::LASTORE:
    case Opcodes::DASTORE: pop(4); break;

    case Opcodes::POP: pop(1); break;

    case Opcodes::POP2: pop(2); break;

    case Opcodes::DUP:
    {
        value1 = pop();
        push(value1);
        push(value1);
        break;
    }

    case Opcodes::DUP_X1:
    {
        value1 = pop();
        value2 = pop();
        push(value1);
        push(value2);
        push(value1);
        break;
    }

    case Opcodes::DUP_X2:
    {
        value1 = pop();
        value2 = pop();
        value3 = pop();
        push(value1);
        push(value3);
        push(value2);
        push(value1);
        break;
    }

    case Opcodes::DUP2:
    {
        value1 = pop();
        value2 = pop();
        push(value2);
        push(value1);
        push(value2);
        push(value1);
        break;
    }

    case Opcodes::DUP2_X1:
    {
        value1 = pop();
        value2 = pop();
        value3 = pop();
        push(value2);
        push(value1);
        push(value3);
        push(value2);
        push(value1);
        break;
    }

    case Opcodes::DUP2_X2:
    {
        value1 = pop();
        value2 = pop();
        value3 = pop();
        t4     = pop();
        push(value2);
        push(value1);
        push(t4);
        push(value3);
        push(value2);
        push(value1);
        break;
    }

    case Opcodes::SWAP:
    {
        value1 = pop();
        value2 = pop();
        push(value1);
        push(value2);
        break;
    }

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
        pop(2);
        push(Opcodes::INTEGER);
        break;

    case Opcodes::LADD:
    case Opcodes::LSUB:
    case Opcodes::LMUL:
    case Opcodes::LDIV:
    case Opcodes::LREM:
    case Opcodes::LSHL:
    case Opcodes::LSHR:
    case Opcodes::LUSHR:
    case Opcodes::LAND:
    case Opcodes::LOR:
    case Opcodes::LXOR:
        pop(4);
        push(Opcodes::LONG);
        push(Opcodes::TOP);
        break;

    case Opcodes::FADD:
    case Opcodes::FSUB:
    case Opcodes::FMUL:
    case Opcodes::FDIV:
    case Opcodes::FREM:
        pop(2);
        push(Opcodes::FLOAT);
        break;

    case Opcodes::DADD:
    case Opcodes::DSUB:
    case Opcodes::DMUL:
    case Opcodes::DDIV:
    case Opcodes::DREM:
        pop(4);
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
        break;

    case Opcodes::LCMP:
    case Opcodes::FCMPL:
    case Opcodes::FCMPG:
    case Opcodes::DCMPL:
    case Opcodes::DCMPG:
        pop(4);
        push(Opcodes::INTEGER);
        break;

    case Opcodes::IFEQ:
    case Opcodes::IFNE:
    case Opcodes::IFLT:
    case Opcodes::IFGE:
    case Opcodes::IFGT:
    case Opcodes::IFLE: pop(1); break;

    case Opcodes::IF_ICMPEQ:
    case Opcodes::IF_ICMPNE:
    case Opcodes::IF_ICMPLT:
    case Opcodes::IF_ICMPGE:
    case Opcodes::IF_ICMPGT:
    case Opcodes::IF_ICMPLE:
    case Opcodes::IF_ACMPEQ:
    case Opcodes::IF_ACMPNE: pop(2); break;

    case Opcodes::I2L:
    case Opcodes::F2L:
    case Opcodes::D2L:
        pop(1);
        push(Opcodes::LONG);
        push(Opcodes::TOP);
        break;

    case Opcodes::I2D:
    case Opcodes::F2D:
        pop(1);
        push(Opcodes::DOUBLE);
        push(Opcodes::TOP);
        break;

    case Opcodes::L2I:
    case Opcodes::F2I:
    case Opcodes::D2I:
        pop(2);
        push(Opcodes::INTEGER);
        break;

    case Opcodes::ARRAYLENGTH:
        pop(1);
        push(Opcodes::INTEGER);
        break;

    case Opcodes::ATHROW:
        pop(1);
        stack_.clear();
        frameDead_ = true;
        break;

    case Opcodes::GETSTATIC: pushDescriptor(stringArg); break;

    case Opcodes::PUTSTATIC: pop(stringArg); break;

    case Opcodes::GETFIELD:
        pop(1);
        pushDescriptor(stringArg);
        break;

    case Opcodes::PUTFIELD:
        pop(stringArg);
        pop(1);
        break;

    case Opcodes::INVOKEVIRTUAL:
    case Opcodes::INVOKESPECIAL:
    case Opcodes::INVOKESTATIC:
    case Opcodes::INVOKEINTERFACE:
    {
        pop(stringArg);
        if (opcode != Opcodes::INVOKESTATIC) {
            value1 = pop();
        }
        pushDescriptor(stringArg);
        labels_.clear();
        break;
    }

    case Opcodes::INVOKEDYNAMIC:
        pop(stringArg);
        pushDescriptor(stringArg);
        labels_.clear();
        break;

    case Opcodes::NEW: push(stringArg); break;

    case Opcodes::NEWARRAY:
        pop(1);
        switch (intArg) {
        case Opcodes::T_BOOLEAN: pushDescriptor(Utf8StringRef::from_cstr("[Z")); break;
        case Opcodes::T_CHAR: pushDescriptor(Utf8StringRef::from_cstr("[C")); break;
        case Opcodes::T_FLOAT: pushDescriptor(Utf8StringRef::from_cstr("[F")); break;
        case Opcodes::T_DOUBLE: pushDescriptor(Utf8StringRef::from_cstr("[D")); break;
        case Opcodes::T_BYTE: pushDescriptor(Utf8StringRef::from_cstr("[B")); break;
        case Opcodes::T_SHORT: pushDescriptor(Utf8StringRef::from_cstr("[S")); break;
        case Opcodes::T_INT: pushDescriptor(Utf8StringRef::from_cstr("[I")); break;
        case Opcodes::T_LONG: pushDescriptor(Utf8StringRef::from_cstr("[J")); break;
        default: throw std::invalid_argument("Invalid array type " + std::to_string(intArg));
        }
        break;

    case Opcodes::ANEWARRAY:
        pop();
        {
            // 字符串线示范(总方案 §5.1):合成描述符优先 intern 进注入的作业 arena;
            // 无注入时回落实例私有 descArena_(#921 staticArena 退役)。
            ca::str::Utf8StringArena& arena = arena_ ? *arena_ : descArena_;
            auto        objDesc = Type::getObjectType(stringArg).getDescriptor(arena);
            pushDescriptor(
                arena.intern(Utf8StringBuilder().append("[").append(objDesc).build().ref()));
        }
        break;

    case Opcodes::CHECKCAST:
        pop();
        // #921:原无参 getDescriptor() 会向进程级 staticArena intern "L...;",现统一
        // 走注入版,落点与 ANEWARRAY 一致(注入 arena 或实例私有 descArena_)。
        pushDescriptor(Type::getObjectType(stringArg).getDescriptor(
            arena_ ? *arena_ : descArena_));
        break;

    case Opcodes::MULTIANEWARRAY:
        pop(intArg);
        pushDescriptor(stringArg);
        break;

    default: throw std::invalid_argument("Invalid opcode " + std::to_string(opcode));
    }
}
