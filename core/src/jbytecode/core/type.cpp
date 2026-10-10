// type.cpp
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/opcodes.hpp"

namespace jbytecode::core {

// 原始类型描述符的静态存储（生命周期 = 整个程序）
namespace {
constexpr ca::u8 kDescV[] = "V";
constexpr ca::u8 kDescZ[] = "Z";
constexpr ca::u8 kDescC[] = "C";
constexpr ca::u8 kDescB[] = "B";
constexpr ca::u8 kDescS[] = "S";
constexpr ca::u8 kDescI[] = "I";
constexpr ca::u8 kDescF[] = "F";
constexpr ca::u8 kDescJ[] = "J";
constexpr ca::u8 kDescD[] = "D";
}   // namespace

const Type Type::VOID_TYPE(VOID, ca::str::Utf8StringRef(kDescV, 1, 1));
const Type Type::BOOLEAN_TYPE(BOOLEAN, ca::str::Utf8StringRef(kDescZ, 1, 1));
const Type Type::CHAR_TYPE(CHAR, ca::str::Utf8StringRef(kDescC, 1, 1));
const Type Type::BYTE_TYPE(BYTE, ca::str::Utf8StringRef(kDescB, 1, 1));
const Type Type::SHORT_TYPE(SHORT, ca::str::Utf8StringRef(kDescS, 1, 1));
const Type Type::INT_TYPE(INT, ca::str::Utf8StringRef(kDescI, 1, 1));
const Type Type::FLOAT_TYPE(FLOAT, ca::str::Utf8StringRef(kDescF, 1, 1));
const Type Type::LONG_TYPE(LONG, ca::str::Utf8StringRef(kDescJ, 1, 1));
const Type Type::DOUBLE_TYPE(DOUBLE, ca::str::Utf8StringRef(kDescD, 1, 1));

Type Type::fromArena(ca::str::Utf8StringArena& arena, const ca::str::Utf8StringRef& typeDescriptor)
{
    ca::str::Utf8StringRef interned = arena.intern(typeDescriptor);
    return getTypeInternal(interned, 0, interned.byte_length());
}

Type Type::getType(const ca::str::Utf8StringRef& typeDescriptor)
{
    return getTypeInternal(typeDescriptor, 0, typeDescriptor.byte_length());
}

Type Type::getObjectType(const ca::str::Utf8StringRef& internalName)
{
    if (internalName.is_empty()) {
        throw std::invalid_argument("empty internal name");
    }
    if (internalName.byte_at(0) == '[') {
        return Type(ARRAY, internalName);
    }
    else {
        return Type(INTERNAL, internalName);
    }
}

Type Type::getMethodType(const ca::str::Utf8StringRef& methodDescriptor)
{
    return Type(METHOD, methodDescriptor);
}

Type Type::getMethodType(ca::str::Utf8StringArena&      arena,
                         const Type&                    returnType,
                         const std::vector<Type>&       argumentTypes)
{
    auto                   desc     = getMethodDescriptor(returnType, argumentTypes);
    ca::str::Utf8StringRef interned = arena.intern(desc.ref());
    return getTypeInternal(interned, 0, interned.byte_length());
}

std::vector<Type> Type::getArgumentTypes(const ca::str::Utf8StringRef& methodDescriptor)
{
    int               numArgs = getArgumentCount(methodDescriptor);
    std::vector<Type> args;
    args.reserve(numArgs);
    size_t cur = 1;
    while (methodDescriptor.byte_at(cur) != ')') {
        size_t start = cur;
        while (methodDescriptor.byte_at(cur) == '[') ++cur;
        if (methodDescriptor.byte_at(cur) == 'L') {
            // 扫描到 ';'
            size_t end = cur;
            while (end < methodDescriptor.byte_length() && methodDescriptor.byte_at(end) != ';')
                ++end;
            if (end >= methodDescriptor.byte_length()) {
                throw std::invalid_argument("Invalid method descriptor: missing ';'");
            }
            cur = end + 1;
        }
        else {
            ++cur;
        }
        args.push_back(getTypeInternal(methodDescriptor, start, cur));
    }
    return args;
}

Type Type::getReturnType(const ca::str::Utf8StringRef& methodDescriptor)
{
    size_t returnOffset = getReturnTypeOffset(methodDescriptor);
    return getTypeInternal(methodDescriptor, returnOffset, methodDescriptor.byte_length());
}

int Type::getArgumentCount(const ca::str::Utf8StringRef& methodDescriptor)
{
    int    count = 0;
    size_t cur   = 1;
    while (methodDescriptor.byte_at(cur) != ')') {
        while (methodDescriptor.byte_at(cur) == '[') ++cur;
        if (methodDescriptor.byte_at(cur) == 'L') {
            size_t end = cur;
            while (end < methodDescriptor.byte_length() && methodDescriptor.byte_at(end) != ';')
                ++end;
            if (end >= methodDescriptor.byte_length()) break;
            cur = end + 1;
        }
        else {
            ++cur;
        }
        ++count;
    }
    return count;
}

int Type::getArgumentsAndReturnSizes(const ca::str::Utf8StringRef& methodDescriptor)
{
    int    argumentsSize = 1;
    size_t cur           = 1;
    while (methodDescriptor.byte_at(cur) != ')') {
        ca::u8 c = methodDescriptor.byte_at(cur);
        if (c == 'J' || c == 'D') {
            argumentsSize += 2;
            ++cur;
        }
        else {
            while (methodDescriptor.byte_at(cur) == '[') ++cur;
            if (methodDescriptor.byte_at(cur) == 'L') {
                size_t end = cur;
                while (end < methodDescriptor.byte_length() && methodDescriptor.byte_at(end) != ';')
                    ++end;
                if (end >= methodDescriptor.byte_length()) break;
                cur = end + 1;
            }
            else {
                ++cur;
            }
            argumentsSize += 1;
        }
    }
    // 返回类型
    ca::u8 ret = methodDescriptor.byte_at(cur + 1);
    if (ret == 'V') {
        return argumentsSize << 2;
    }
    else {
        int returnSize = (ret == 'J' || ret == 'D') ? 2 : 1;
        return (argumentsSize << 2) | returnSize;
    }
}

ca::str::Utf8String Type::getMethodDescriptor(const Type&              returnType,
                                              const std::vector<Type>& argumentTypes)
{
    ca::str::Utf8StringBuilder buf;
    buf.append("(");
    for (const auto& arg : argumentTypes) {
        arg.appendDescriptor(buf);
    }
    buf.append(")");
    returnType.appendDescriptor(buf);
    return buf.build();
}

ca::str::Utf8String Type::getInternalName(const ca::str::Utf8StringRef& binaryName)
{
    ca::str::Utf8StringBuilder result;
    for (ca::usize i = 0; i < binaryName.byte_length(); ++i) {
        ca::u8 c = binaryName.byte_at(i);
        if (c == '.')
            result.append_code_point('/');
        else
            result.append_code_point(c);
    }
    return result.build();
}

int Type::getSort() const
{
    return (sort_ == INTERNAL) ? OBJECT : sort_;
}

int Type::getDimensions() const
{
    if (sort_ != ARRAY) return 0;
    int dims = 1;
    while (dims < static_cast<int>(value_.byte_length()) && value_.byte_at(dims) == '[') ++dims;
    return dims;
}

Type Type::getElementType() const
{
    if (sort_ != ARRAY) {
        throw std::logic_error("getElementType() called on non-array type");
    }
    int dims = getDimensions();
    return getTypeInternal(value_, dims, value_.byte_length());
}

ca::str::Utf8StringRef Type::getClassName(ca::str::Utf8StringArena& arena) const
{
    switch (sort_) {
    case VOID: return arena.intern("void");
    case BOOLEAN: return arena.intern("boolean");
    case CHAR: return arena.intern("char");
    case BYTE: return arena.intern("byte");
    case SHORT: return arena.intern("short");
    case INT: return arena.intern("int");
    case FLOAT: return arena.intern("float");
    case LONG: return arena.intern("long");
    case DOUBLE: return arena.intern("double");
    case ARRAY:
    {
        Type                       elem = getElementType();
        ca::str::Utf8StringBuilder cn;
        cn.append(elem.getClassName(arena));
        int dims = getDimensions();
        for (int i = 0; i < dims; ++i) cn.append("[]");
        return arena.intern(cn.build().ref());
    }
    case OBJECT:
    case INTERNAL:
    {
        ca::str::Utf8StringBuilder name;
        for (ca::usize i = 0; i < value_.byte_length(); ++i) {
            ca::u8 c = value_.byte_at(i);
            name.append_code_point(c == '/' ? '.' : c);
        }
        return arena.intern(name.build().ref());
    }
    }
    throw std::logic_error("Unknown sort");
}

ca::str::Utf8StringRef Type::getInternalName(ca::str::Utf8StringArena& arena) const
{
    if (sort_ == OBJECT || sort_ == INTERNAL) {
        return value_;
    }
    else if (sort_ == ARRAY) {
        return arena.intern(value_);
    }
    else {
        throw std::logic_error("getInternalName() called on non-reference type");
    }
}

ca::str::Utf8StringRef Type::getDescriptor(ca::str::Utf8StringArena& arena) const
{
    if (sort_ == OBJECT || sort_ == INTERNAL) {
        ca::str::Utf8StringBuilder desc;
        desc.append("L").append(value_).append(";");
        return arena.intern(desc.build().ref());
    }
    else {
        return value_;
    }
}

int Type::getSize() const
{
    switch (sort_) {
    case VOID: return 0;
    case LONG:
    case DOUBLE: return 2;
    default: return 1;
    }
}

int Type::getArgumentCount() const
{
    if (sort_ != METHOD) {
        throw std::logic_error("getArgumentCount() called on non-method type");
    }
    // METHOD 排序的描述符就是 value_ 本身（#921 staticArena 退役：不再经无参
    // getDescriptor() 转调），零 intern、零拷贝。
    return getArgumentCount(value_);
}

std::vector<Type> Type::getArgumentTypes() const
{
    if (sort_ != METHOD) {
        throw std::logic_error("getArgumentTypes() called on non-method type");
    }
    return getArgumentTypes(value_);
}

Type Type::getReturnType() const
{
    if (sort_ != METHOD) {
        throw std::logic_error("getReturnType() called on non-method type");
    }
    return getReturnType(value_);
}

int Type::getArgumentsAndReturnSizes() const
{
    if (sort_ != METHOD) {
        throw std::logic_error("getArgumentsAndReturnSizes() called on non-method type");
    }
    return getArgumentsAndReturnSizes(value_);
}

int Type::getOpcode(int opcode) const
{
    if (opcode == Opcodes::IALOAD || opcode == Opcodes::IASTORE) {
        switch (getSort()) {
        case BOOLEAN:
        case BYTE: return opcode + (Opcodes::BALOAD - Opcodes::IALOAD);
        case CHAR: return opcode + (Opcodes::CALOAD - Opcodes::IALOAD);
        case SHORT: return opcode + (Opcodes::SALOAD - Opcodes::IALOAD);
        case INT: return opcode;
        case FLOAT: return opcode + (Opcodes::FALOAD - Opcodes::IALOAD);
        case LONG: return opcode + (Opcodes::LALOAD - Opcodes::IALOAD);
        case DOUBLE: return opcode + (Opcodes::DALOAD - Opcodes::IALOAD);
        case ARRAY:
        case OBJECT: return opcode + (Opcodes::AALOAD - Opcodes::IALOAD);
        default: throw std::invalid_argument("Invalid type for IALOAD/IASTORE");
        }
    }
    else {
        switch (getSort()) {
        case VOID:
            if (opcode != Opcodes::IRETURN) throw std::invalid_argument("Invalid opcode for void");
            return Opcodes::RETURN;
        case BOOLEAN:
        case BYTE:
        case CHAR:
        case SHORT:
        case INT: return opcode;
        case FLOAT: return opcode + (Opcodes::FRETURN - Opcodes::IRETURN);
        case LONG: return opcode + (Opcodes::LRETURN - Opcodes::IRETURN);
        case DOUBLE: return opcode + (Opcodes::DRETURN - Opcodes::IRETURN);
        case ARRAY:
        case OBJECT:
            if (opcode != Opcodes::ILOAD && opcode != Opcodes::ISTORE &&
                opcode != Opcodes::IRETURN) {
                throw std::invalid_argument("Invalid opcode for reference type");
            }
            return opcode + (Opcodes::ARETURN - Opcodes::IRETURN);
        default: throw std::invalid_argument("Invalid type for opcode adjustment");
        }
    }
}

bool Type::operator==(const Type& other) const
{
    if (this == &other) return true;
    int s1 = (sort_ == INTERNAL) ? OBJECT : sort_;
    int s2 = (other.sort_ == INTERNAL) ? OBJECT : other.sort_;
    if (s1 != s2) return false;
    return value_.equals(other.value_);
}

bool Type::operator!=(const Type& other) const
{
    return !(*this == other);
}

std::string Type::toString() const
{
    // #921 staticArena 退役：经 appendDescriptor 直接拼描述符，不 intern 任何进程级/
    // 局部池（原实现经无参 getDescriptor() 会把 OBJECT 排序类名 immortal 化）。
    ca::str::Utf8StringBuilder buf;
    appendDescriptor(buf);
    auto d = buf.build();
    return std::string(reinterpret_cast<const char*>(d.data()), d.byte_length());
}

Type Type::getTypeInternal(const ca::str::Utf8StringRef& desc, size_t begin, size_t end)
{
    if (begin >= end) throw std::invalid_argument("Empty descriptor");
    ca::u8 c = desc.byte_at(begin);
    switch (c) {
    case 'V': return VOID_TYPE;
    case 'Z': return BOOLEAN_TYPE;
    case 'C': return CHAR_TYPE;
    case 'B': return BYTE_TYPE;
    case 'S': return SHORT_TYPE;
    case 'I': return INT_TYPE;
    case 'F': return FLOAT_TYPE;
    case 'J': return LONG_TYPE;
    case 'D': return DOUBLE_TYPE;
    case '[': return Type(ARRAY, desc.slice(begin, end));
    case 'L': return Type(OBJECT, desc.slice(begin + 1, end - 1));
    case '(': return Type(METHOD, desc.slice(begin, end));
    default:
    {
        std::string msg = "Invalid descriptor: ";
        msg.append(reinterpret_cast<const char*>(desc.data()), desc.byte_length());
        throw std::invalid_argument(msg);
    }
    }
}

size_t Type::getReturnTypeOffset(const ca::str::Utf8StringRef& methodDescriptor)
{
    size_t cur = 1;
    while (methodDescriptor.byte_at(cur) != ')') {
        while (methodDescriptor.byte_at(cur) == '[') ++cur;
        if (methodDescriptor.byte_at(cur) == 'L') {
            size_t e = cur;
            while (e < methodDescriptor.byte_length() && methodDescriptor.byte_at(e) != ';') ++e;
            if (e >= methodDescriptor.byte_length()) break;
            cur = e + 1;
        }
        else {
            ++cur;
        }
    }
    return cur + 1;
}

void Type::appendDescriptor(ca::str::Utf8StringBuilder& out) const
{
    if (sort_ == OBJECT || sort_ == INTERNAL) {
        out.append("L");
        out.append(value_);
        out.append(";");
    }
    else {
        out.append(value_);
    }
}

}   // namespace jbytecode::core
