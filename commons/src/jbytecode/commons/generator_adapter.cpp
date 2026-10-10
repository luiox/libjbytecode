#include "jbytecode/commons/generator_adapter.hpp"

#include <cstring>
#include <stdexcept>

#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/primitives.hpp"

using namespace jbytecode::commons;
using namespace jbytecode::core;

namespace {

ca::str::Utf8StringRef cstr(const char* s)
{
    return ca::str::Utf8StringRef(
        reinterpret_cast<const ca::u8*>(s), std::strlen(s), std::strlen(s));
}

// #1068 R19-F1：internal name 的 owning 深拷贝——返回值必须能跨函数边界
// 存活（5 参构造函数把它交给 visitMethod，消费点在另一帧）。旧实现（#921
// 批次2）返回指向函数级局部 scratch 的 ref，ARRAY 排序项在 return 后即悬垂；
// 当时的"字节瞬时消费"裁决表述与实况不符（消费发生在 scratch 死后，见
// R19-F1）。合法 JVM 异常恒为 class 排序（此路径当前不可达），但本函数
// 服务 public 构造函数，按 owning 返回根治。scratch 仅为 intern 落点，
// 字节在 scratch 析构前已拷出，作用域外可用。
std::vector<ca::str::Utf8String> internalNamesOwned(const std::vector<Type>& types)
{
    ca::str::Utf8StringArena         scratch;
    std::vector<ca::str::Utf8String> names;
    names.reserve(types.size());
    for (const auto& t : types) {
        const ca::str::Utf8StringRef internal = t.getInternalName(scratch);
        names.emplace_back(internal.data(), internal.byte_length());
    }
    return names;
}

// #1068 R19-F1：5 参构造函数的 visitMethod 桥。owning 串与本帧的 ref 视图
// 同帧共存，visitMethod 在帧内完成消费（下游按合同自行重新 intern）——
// 跨函数边界零悬垂。
jbytecode::core::MethodVisitor* visitMethodWithExceptions(
    int                            access,
    const Method&                  method,
    ca::str::Utf8StringRef         signature,
    const std::vector<Type>&       exceptions,
    jbytecode::core::ClassVisitor* classVisitor)
{
    const std::vector<ca::str::Utf8String> owned = internalNamesOwned(exceptions);
    std::vector<ca::str::Utf8StringRef>    names;
    names.reserve(owned.size());
    for (const auto& s : owned) {
        names.push_back(s.ref());
    }
    return classVisitor->visitMethod(
        access, method.getName(), method.getDescriptor(), signature, names);
}

}   // namespace

// -----------------------------------------------------------------------
// Static members
// -----------------------------------------------------------------------

const ca::str::Utf8String GeneratorAdapter::CLASS_DESCRIPTOR =
    ca::str::Utf8String(reinterpret_cast<const ca::u8*>("Ljava/lang/Class;"), 16);

const Type GeneratorAdapter::BYTE_TYPE      = Type::getObjectType(cstr("java/lang/Byte"));
const Type GeneratorAdapter::BOOLEAN_TYPE   = Type::getObjectType(cstr("java/lang/Boolean"));
const Type GeneratorAdapter::SHORT_TYPE     = Type::getObjectType(cstr("java/lang/Short"));
const Type GeneratorAdapter::CHARACTER_TYPE = Type::getObjectType(cstr("java/lang/Character"));
const Type GeneratorAdapter::INTEGER_TYPE   = Type::getObjectType(cstr("java/lang/Integer"));
const Type GeneratorAdapter::FLOAT_TYPE     = Type::getObjectType(cstr("java/lang/Float"));
const Type GeneratorAdapter::LONG_TYPE      = Type::getObjectType(cstr("java/lang/Long"));
const Type GeneratorAdapter::DOUBLE_TYPE    = Type::getObjectType(cstr("java/lang/Double"));
const Type GeneratorAdapter::NUMBER_TYPE    = Type::getObjectType(cstr("java/lang/Number"));
const Type GeneratorAdapter::OBJECT_TYPE    = Type::getObjectType(cstr("java/lang/Object"));

const Method GeneratorAdapter::BOOLEAN_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("boolean booleanValue()"));
const Method GeneratorAdapter::CHAR_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("char charValue()"));
const Method GeneratorAdapter::INT_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("int intValue()"));
const Method GeneratorAdapter::FLOAT_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("float floatValue()"));
const Method GeneratorAdapter::LONG_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("long longValue()"));
const Method GeneratorAdapter::DOUBLE_VALUE =
    Method::getMethod(ca::str::ZUtf8StringRef::from_static("double doubleValue()"));

// -----------------------------------------------------------------------
// Constructors
// -----------------------------------------------------------------------

GeneratorAdapter::GeneratorAdapter(MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                                   ca::str::Utf8StringRef descriptor)
    : GeneratorAdapter(Opcodes::ASM9, mv, access, name, descriptor)
{}

GeneratorAdapter::GeneratorAdapter(int api, MethodVisitor* mv, int access,
                                   ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor)
    : LocalVariablesSorter(api, access, descriptor, mv)
    , access_(access)
    , name_(name.data(), name.byte_length())
    , returnType_(Type::getReturnType(descriptor))
    , argumentTypes_(Type::getArgumentTypes(descriptor))
{}

GeneratorAdapter::GeneratorAdapter(int access, const Method& method, MethodVisitor* mv)
    : GeneratorAdapter(mv, access, method.getName(), method.getDescriptor())
{}

GeneratorAdapter::GeneratorAdapter(int access, const Method& method,
                                   ca::str::Utf8StringRef   signature,
                                   const std::vector<Type>& exceptions, ClassVisitor* classVisitor)
    : GeneratorAdapter(access, method,
                       visitMethodWithExceptions(access, method, signature, exceptions, classVisitor))
{}

// -----------------------------------------------------------------------
// Private helpers
// -----------------------------------------------------------------------

Type GeneratorAdapter::getBoxedType(const Type& type)
{
    switch (type.getSort()) {
    case Type::BYTE: return BYTE_TYPE;
    case Type::BOOLEAN: return BOOLEAN_TYPE;
    case Type::SHORT: return SHORT_TYPE;
    case Type::CHAR: return CHARACTER_TYPE;
    case Type::INT: return INTEGER_TYPE;
    case Type::FLOAT: return FLOAT_TYPE;
    case Type::LONG: return LONG_TYPE;
    case Type::DOUBLE: return DOUBLE_TYPE;
    default: return type;
    }
}

int GeneratorAdapter::getArgIndex(int arg)
{
    int index = (access_ & Opcodes::ACC_STATIC) == 0 ? 1 : 0;
    for (int i = 0; i < arg; i++) {
        index += argumentTypes_[i].getSize();
    }
    return index;
}

void GeneratorAdapter::loadInsn(const Type& type, int index)
{
    mv->visitVarInsn(type.getOpcode(Opcodes::ILOAD), index);
}

void GeneratorAdapter::storeInsn(const Type& type, int index)
{
    mv->visitVarInsn(type.getOpcode(Opcodes::ISTORE), index);
}

void GeneratorAdapter::fieldInsn(int opcode, const Type& ownerType, ca::str::Utf8StringRef name,
                                 const Type& fieldType)
{
    // #921 staticArena 退役：字节瞬时传给 visitFieldInsn，函数级局部 arena 零驻留。
    ca::str::Utf8StringArena scratch;
    mv->visitFieldInsn(opcode,
                       ownerType.getInternalName(scratch),
                       name,
                       fieldType.getDescriptor(scratch));
}

void GeneratorAdapter::invokeInsn(int opcode, const Type& type, const Method& method,
                                  bool isInterface)
{
    ca::str::Utf8StringArena scratch;   // #921 staticArena 退役：同上，瞬时消费零驻留。
    ca::str::Utf8StringRef   owner =
        type.getSort() == Type::ARRAY ? type.getDescriptor(scratch) : type.getInternalName(scratch);
    mv->visitMethodInsn(opcode, owner, method.getName(), method.getDescriptor(), isInterface);
}

void GeneratorAdapter::typeInsn(int opcode, const Type& type)
{
    ca::str::Utf8StringArena scratch;   // #921 staticArena 退役：同上，瞬时消费零驻留。
    mv->visitTypeInsn(opcode, type.getInternalName(scratch));
}

// -----------------------------------------------------------------------
// push()
// -----------------------------------------------------------------------

void GeneratorAdapter::push(bool value)
{
    push(value ? 1 : 0);
}

void GeneratorAdapter::push(int value)
{
    if (value >= -1 && value <= 5) {
        mv->visitInsn(Opcodes::ICONST_0 + value);
    }
    else if (value >= -128 && value <= 127) {
        mv->visitIntInsn(Opcodes::BIPUSH, value);
    }
    else if (value >= -32768 && value <= 32767) {
        mv->visitIntInsn(Opcodes::SIPUSH, value);
    }
    else {
        mv->visitLdcInsn(ca::core::Any(value));
    }
}

void GeneratorAdapter::push(long value)
{
    if (value == 0L || value == 1L) {
        mv->visitInsn(Opcodes::LCONST_0 + static_cast<int>(value));
    }
    else {
        mv->visitLdcInsn(ca::core::Any(static_cast<int64_t>(value)));
    }
}

void GeneratorAdapter::push(float value)
{
    int32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if (bits == 0 || bits == 0x3F800000 || bits == 0x40000000) {
        mv->visitInsn(Opcodes::FCONST_0 + static_cast<int>(value));
    }
    else {
        mv->visitLdcInsn(ca::core::Any(value));
    }
}

void GeneratorAdapter::push(double value)
{
    int64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    if (bits == 0 || bits == 0x3FF0000000000000LL) {
        mv->visitInsn(Opcodes::DCONST_0 + static_cast<int>(value));
    }
    else {
        mv->visitLdcInsn(ca::core::Any(value));
    }
}

void GeneratorAdapter::push(ca::str::Utf8StringRef value)
{
    mv->visitLdcInsn(ca::core::Any(value));
}

void GeneratorAdapter::push(const Type& value)
{
    if (value.getSort() == Type::VOID) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Void"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::BOOLEAN) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Boolean"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::CHAR) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Character"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::BYTE) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Byte"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::SHORT) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Short"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::INT) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Integer"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::FLOAT) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Float"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::LONG) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Long"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else if (value.getSort() == Type::DOUBLE) {
        mv->visitFieldInsn(
            Opcodes::GETSTATIC, cstr("java/lang/Double"), cstr("TYPE"), CLASS_DESCRIPTOR.ref());
    }
    else {
        mv->visitLdcInsn(ca::core::Any(value));
    }
}

void GeneratorAdapter::push(const Handle& handle)
{
    mv->visitLdcInsn(ca::core::Any(handle));
}

void GeneratorAdapter::push(const ConstantDynamic& constantDynamic)
{
    mv->visitLdcInsn(ca::core::Any(constantDynamic));
}

// -----------------------------------------------------------------------
// load/store args
// -----------------------------------------------------------------------

void GeneratorAdapter::loadThis()
{
    if ((access_ & Opcodes::ACC_STATIC) != 0) {
        throw std::logic_error("no 'this' pointer within static method");
    }
    mv->visitVarInsn(Opcodes::ALOAD, 0);
}

void GeneratorAdapter::loadArg(int arg)
{
    loadInsn(argumentTypes_[arg], getArgIndex(arg));
}

void GeneratorAdapter::loadArgs(int arg, int count)
{
    int index = getArgIndex(arg);
    for (int i = 0; i < count; ++i) {
        Type argumentType = argumentTypes_[arg + i];
        loadInsn(argumentType, index);
        index += argumentType.getSize();
    }
}

void GeneratorAdapter::loadArgs()
{
    loadArgs(0, static_cast<int>(argumentTypes_.size()));
}

void GeneratorAdapter::loadArgArray()
{
    push(static_cast<int>(argumentTypes_.size()));
    newArray(OBJECT_TYPE);
    for (int i = 0; i < static_cast<int>(argumentTypes_.size()); i++) {
        dup();
        push(i);
        loadArg(i);
        box(argumentTypes_[i]);
        arrayStore(OBJECT_TYPE);
    }
}

void GeneratorAdapter::storeArg(int arg)
{
    storeInsn(argumentTypes_[arg], getArgIndex(arg));
}

// -----------------------------------------------------------------------
// Local variables
// -----------------------------------------------------------------------

Type GeneratorAdapter::getLocalType(int local)
{
    return localTypes_[local - firstLocal_];
}

void GeneratorAdapter::setLocalType(int local, const Type& type)
{
    int index = local - firstLocal_;
    while (static_cast<int>(localTypes_.size()) < index + 1) {
        localTypes_.push_back(Type::VOID_TYPE);
    }
    localTypes_[index] = type;
}

void GeneratorAdapter::loadLocal(int local)
{
    loadInsn(getLocalType(local), local);
}

void GeneratorAdapter::loadLocal(int local, const Type& type)
{
    setLocalType(local, type);
    loadInsn(type, local);
}

void GeneratorAdapter::storeLocal(int local)
{
    storeInsn(getLocalType(local), local);
}

void GeneratorAdapter::storeLocal(int local, const Type& type)
{
    setLocalType(local, type);
    storeInsn(type, local);
}

// -----------------------------------------------------------------------
// Array load/store
// -----------------------------------------------------------------------

void GeneratorAdapter::arrayLoad(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IALOAD));
}

void GeneratorAdapter::arrayStore(const Type& type)
{
    mv->visitInsn(type.getOpcode(Opcodes::IASTORE));
}

// -----------------------------------------------------------------------
// Stack operations
// -----------------------------------------------------------------------

void GeneratorAdapter::pop()
{
    mv->visitInsn(Opcodes::POP);
}

void GeneratorAdapter::pop2()
{
    mv->visitInsn(Opcodes::POP2);
}

void GeneratorAdapter::dup()
{
    mv->visitInsn(Opcodes::DUP);
}

void GeneratorAdapter::dup2()
{
    mv->visitInsn(Opcodes::DUP2);
}

void GeneratorAdapter::dupX1()
{
    mv->visitInsn(Opcodes::DUP_X1);
}

void GeneratorAdapter::dupX2()
{
    mv->visitInsn(Opcodes::DUP_X2);
}

void GeneratorAdapter::dup2X1()
{
    mv->visitInsn(Opcodes::DUP2_X1);
}

void GeneratorAdapter::dup2X2()
{
    mv->visitInsn(Opcodes::DUP2_X2);
}

void GeneratorAdapter::swap()
{
    mv->visitInsn(Opcodes::SWAP);
}

void GeneratorAdapter::swap(const Type& prev, const Type& type)
{
    if (type.getSize() == 1) {
        if (prev.getSize() == 1) {
            swap();
        }
        else {
            dupX2();
            pop();
        }
    }
    else {
        if (prev.getSize() == 1) {
            dup2X1();
            pop2();
        }
        else {
            dup2X2();
            pop2();
        }
    }
}

// -----------------------------------------------------------------------
// Math
// -----------------------------------------------------------------------

void GeneratorAdapter::math(int op, const Type& type)
{
    mv->visitInsn(type.getOpcode(op));
}

void GeneratorAdapter::not_()
{
    mv->visitInsn(Opcodes::ICONST_1);
    mv->visitInsn(Opcodes::IXOR);
}

void GeneratorAdapter::iinc(int local, int amount)
{
    mv->visitIincInsn(local, amount);
}

// -----------------------------------------------------------------------
// Cast
// -----------------------------------------------------------------------

void GeneratorAdapter::cast(const Type& from, const Type& to)
{
    if (from != to) {
        if (from.getSort() < Type::BOOLEAN || from.getSort() > Type::DOUBLE ||
            to.getSort() < Type::BOOLEAN || to.getSort() > Type::DOUBLE) {
            throw std::invalid_argument("Cannot cast from " + from.toString() + " to " +
                                        to.toString());
        }
        InstructionAdapter::cast(mv, from, to);
    }
}

// -----------------------------------------------------------------------
// Boxing/unboxing
// -----------------------------------------------------------------------

void GeneratorAdapter::box(const Type& type)
{
    if (type.getSort() == Type::OBJECT || type.getSort() == Type::ARRAY) {
        return;
    }
    if (type == Type::VOID_TYPE) {
        mv->visitInsn(Opcodes::ACONST_NULL);
    }
    else {
        Type boxedType = getBoxedType(type);
        newInstance(boxedType);
        if (type.getSize() == 2) {
            dupX2();
            dupX2();
            pop();
        }
        else {
            dupX1();
            swap();
        }
        invokeConstructor(boxedType,
                          Method(cstr("<init>"), Type::VOID_TYPE, std::vector<Type>{type}));
    }
}

void GeneratorAdapter::valueOf(const Type& type)
{
    if (type.getSort() == Type::OBJECT || type.getSort() == Type::ARRAY) {
        return;
    }
    if (type == Type::VOID_TYPE) {
        mv->visitInsn(Opcodes::ACONST_NULL);
    }
    else {
        Type boxedType = getBoxedType(type);
        invokeStatic(boxedType, Method(cstr("valueOf"), boxedType, std::vector<Type>{type}));
    }
}

void GeneratorAdapter::unbox(const Type& type)
{
    Type          boxedType   = NUMBER_TYPE;
    const Method* unboxMethod = nullptr;

    switch (type.getSort()) {
    case Type::VOID: return;
    case Type::CHAR:
        boxedType   = CHARACTER_TYPE;
        unboxMethod = &CHAR_VALUE;
        break;
    case Type::BOOLEAN:
        boxedType   = BOOLEAN_TYPE;
        unboxMethod = &BOOLEAN_VALUE;
        break;
    case Type::DOUBLE: unboxMethod = &DOUBLE_VALUE; break;
    case Type::FLOAT: unboxMethod = &FLOAT_VALUE; break;
    case Type::LONG: unboxMethod = &LONG_VALUE; break;
    case Type::INT:
    case Type::SHORT:
    case Type::BYTE: unboxMethod = &INT_VALUE; break;
    default: unboxMethod = nullptr; break;
    }

    if (unboxMethod == nullptr) {
        checkCast(type);
    }
    else {
        checkCast(boxedType);
        invokeVirtual(boxedType, *unboxMethod);
    }
}

// -----------------------------------------------------------------------
// Labels and jumps
// -----------------------------------------------------------------------

Label* GeneratorAdapter::newLabel()
{
    return labelArena_.create<Label>();
}

void GeneratorAdapter::mark(Label* label)
{
    mv->visitLabel(label);
}

Label* GeneratorAdapter::mark()
{
    Label* label = labelArena_.create<Label>();
    mv->visitLabel(label);
    return label;
}

void GeneratorAdapter::ifCmp(const Type& type, int mode, Label* label)
{
    switch (type.getSort()) {
    case Type::LONG: mv->visitInsn(Opcodes::LCMP); break;
    case Type::DOUBLE:
        mv->visitInsn(mode == GE || mode == GT ? Opcodes::DCMPL : Opcodes::DCMPG);
        break;
    case Type::FLOAT:
        mv->visitInsn(mode == GE || mode == GT ? Opcodes::FCMPL : Opcodes::FCMPG);
        break;
    case Type::ARRAY:
    case Type::OBJECT:
        if (mode == EQ) {
            mv->visitJumpInsn(Opcodes::IF_ACMPEQ, label);
            return;
        }
        else if (mode == NE) {
            mv->visitJumpInsn(Opcodes::IF_ACMPNE, label);
            return;
        }
        else {
            throw std::invalid_argument("Bad comparison for type " + type.toString());
        }
    default:
    {
        int intOp = -1;
        switch (mode) {
        case EQ: intOp = Opcodes::IF_ICMPEQ; break;
        case NE: intOp = Opcodes::IF_ICMPNE; break;
        case GE: intOp = Opcodes::IF_ICMPGE; break;
        case LT: intOp = Opcodes::IF_ICMPLT; break;
        case LE: intOp = Opcodes::IF_ICMPLE; break;
        case GT: intOp = Opcodes::IF_ICMPGT; break;
        default: throw std::invalid_argument("Bad comparison mode " + std::to_string(mode));
        }
        mv->visitJumpInsn(intOp, label);
        return;
    }
    }
    mv->visitJumpInsn(mode, label);
}

void GeneratorAdapter::ifICmp(int mode, Label* label)
{
    ifCmp(Type::INT_TYPE, mode, label);
}

void GeneratorAdapter::ifZCmp(int mode, Label* label)
{
    mv->visitJumpInsn(mode, label);
}

void GeneratorAdapter::ifNull(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFNULL, label);
}

void GeneratorAdapter::ifNonNull(Label* label)
{
    mv->visitJumpInsn(Opcodes::IFNONNULL, label);
}

void GeneratorAdapter::goTo(Label* label)
{
    mv->visitJumpInsn(Opcodes::GOTO, label);
}

void GeneratorAdapter::ret(int local)
{
    mv->visitVarInsn(Opcodes::RET, local);
}

void GeneratorAdapter::tableSwitch(const std::vector<int>& keys, TableSwitchGenerator* generator)
{
    float density;
    if (keys.empty()) {
        density = 0;
    }
    else {
        density = static_cast<float>(keys.size()) / (keys.back() - keys.front() + 1);
    }
    tableSwitch(keys, generator, density >= 0.5f);
}

void GeneratorAdapter::tableSwitch(const std::vector<int>& keys, TableSwitchGenerator* generator,
                                   bool useTable)
{
    for (size_t i = 1; i < keys.size(); ++i) {
        if (keys[i] < keys[i - 1]) {
            throw std::invalid_argument("keys must be sorted in ascending order");
        }
    }
    Label* defaultLabel = newLabel();
    Label* endLabel     = newLabel();
    if (!keys.empty()) {
        size_t numKeys = keys.size();
        if (useTable) {
            int                 min   = keys[0];
            int                 max   = keys[numKeys - 1];
            int                 range = max - min + 1;
            std::vector<Label*> labels(range, defaultLabel);
            for (size_t i = 0; i < numKeys; ++i) {
                labels[keys[i] - min] = newLabel();
            }
            mv->visitTableSwitchInsn(min, max, defaultLabel, labels);
            for (int i = 0; i < range; ++i) {
                Label* label = labels[i];
                if (label != defaultLabel) {
                    mark(label);
                    generator->generateCase(i + min, endLabel);
                }
            }
        }
        else {
            std::vector<Label*> labels(numKeys);
            for (size_t i = 0; i < numKeys; ++i) {
                labels[i] = newLabel();
            }
            mv->visitLookupSwitchInsn(defaultLabel, keys, labels);
            for (size_t i = 0; i < numKeys; ++i) {
                mark(labels[i]);
                generator->generateCase(keys[i], endLabel);
            }
        }
    }
    mark(defaultLabel);
    generator->generateDefault();
    mark(endLabel);
}

// -----------------------------------------------------------------------
// Return
// -----------------------------------------------------------------------

void GeneratorAdapter::returnValue()
{
    mv->visitInsn(returnType_.getOpcode(Opcodes::IRETURN));
}

// -----------------------------------------------------------------------
// Fields
// -----------------------------------------------------------------------

void GeneratorAdapter::getStatic(const Type& owner, ca::str::Utf8StringRef name, const Type& type)
{
    fieldInsn(Opcodes::GETSTATIC, owner, name, type);
}

void GeneratorAdapter::putStatic(const Type& owner, ca::str::Utf8StringRef name, const Type& type)
{
    fieldInsn(Opcodes::PUTSTATIC, owner, name, type);
}

void GeneratorAdapter::getField(const Type& owner, ca::str::Utf8StringRef name, const Type& type)
{
    fieldInsn(Opcodes::GETFIELD, owner, name, type);
}

void GeneratorAdapter::putField(const Type& owner, ca::str::Utf8StringRef name, const Type& type)
{
    fieldInsn(Opcodes::PUTFIELD, owner, name, type);
}

// -----------------------------------------------------------------------
// Invocation
// -----------------------------------------------------------------------

void GeneratorAdapter::invokeVirtual(const Type& owner, const Method& method)
{
    invokeInsn(Opcodes::INVOKEVIRTUAL, owner, method, false);
}

void GeneratorAdapter::invokeConstructor(const Type& type, const Method& method)
{
    invokeInsn(Opcodes::INVOKESPECIAL, type, method, false);
}

void GeneratorAdapter::invokeStatic(const Type& owner, const Method& method)
{
    invokeInsn(Opcodes::INVOKESTATIC, owner, method, false);
}

void GeneratorAdapter::invokeInterface(const Type& owner, const Method& method)
{
    invokeInsn(Opcodes::INVOKEINTERFACE, owner, method, true);
}

void GeneratorAdapter::invokeDynamic(ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                                     const Handle&                     bootstrapMethodHandle,
                                     const std::vector<ca::core::Any>& bootstrapMethodArguments)
{
    mv->visitInvokeDynamicInsn(name, descriptor, bootstrapMethodHandle, bootstrapMethodArguments);
}

// -----------------------------------------------------------------------
// Object/array creation
// -----------------------------------------------------------------------

void GeneratorAdapter::newInstance(const Type& type)
{
    typeInsn(Opcodes::NEW, type);
}

void GeneratorAdapter::newArray(const Type& type)
{
    InstructionAdapter::newarray(mv, type);
}

// -----------------------------------------------------------------------
// Misc
// -----------------------------------------------------------------------

void GeneratorAdapter::arrayLength()
{
    mv->visitInsn(Opcodes::ARRAYLENGTH);
}

void GeneratorAdapter::throwException()
{
    mv->visitInsn(Opcodes::ATHROW);
}

void GeneratorAdapter::throwException(const Type& type, ca::str::Utf8StringRef message)
{
    newInstance(type);
    dup();
    push(message);
    invokeConstructor(
        type, Method::getMethod(ca::str::ZUtf8StringRef::from_static("void <init> (String)")));
    throwException();
}

void GeneratorAdapter::checkCast(const Type& type)
{
    if (type != OBJECT_TYPE) {
        typeInsn(Opcodes::CHECKCAST, type);
    }
}

void GeneratorAdapter::instanceOf(const Type& type)
{
    typeInsn(Opcodes::INSTANCEOF, type);
}

void GeneratorAdapter::monitorEnter()
{
    mv->visitInsn(Opcodes::MONITORENTER);
}

void GeneratorAdapter::monitorExit()
{
    mv->visitInsn(Opcodes::MONITOREXIT);
}

// -----------------------------------------------------------------------
// Non-instruction
// -----------------------------------------------------------------------

void GeneratorAdapter::endMethod()
{
    if ((access_ & Opcodes::ACC_ABSTRACT) == 0) {
        mv->visitMaxs(0, 0);
    }
    mv->visitEnd();
}

void GeneratorAdapter::catchException(Label* start, Label* end, const Type& exception)
{
    Label* catchLabel = labelArena_.create<Label>();
    if (exception.getSort() == Type::VOID) {
        mv->visitTryCatchBlock(start, end, catchLabel, cstr(""));
    }
    else {
        // #921 staticArena 退役：异常内部名瞬时传递，函数级局部 arena 零驻留。
        ca::str::Utf8StringArena scratch;
        mv->visitTryCatchBlock(start, end, catchLabel, exception.getInternalName(scratch));
    }
    mark(catchLabel);
}
