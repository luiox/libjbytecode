#include "jbytecode/analysis/simple_verifier.hpp"
#include "libca/str/utf8_string.hpp"

#include <cstring>

using namespace jbytecode::analysis;
using namespace jbytecode::core;
using ca::str::Utf8StringRef;

const Type SimpleVerifier::OBJECT_TYPE =
    Type::getObjectType(Utf8StringRef::from_cstr("java/lang/Object"));

SimpleVerifier::SimpleVerifier()
    : SimpleVerifier(Type{}, Type{}, {}, false)
{}

SimpleVerifier::SimpleVerifier(const Type& currentClass, const Type& currentSuperClass,
                               bool isInterface)
    : SimpleVerifier(currentClass, currentSuperClass, {}, isInterface)
{}

SimpleVerifier::SimpleVerifier(const Type& currentClass, const Type& currentSuperClass,
                               const std::vector<Type>& currentClassInterfaces, bool isInterface)
    : SimpleVerifier(Opcodes::ASM9, currentClass, currentSuperClass, currentClassInterfaces,
                     isInterface)
{}

SimpleVerifier::SimpleVerifier(int api, const Type& currentClass, const Type& currentSuperClass,
                               const std::vector<Type>& currentClassInterfaces, bool isInterface)
    : BasicVerifier()
    , currentClass_(currentClass)
    , currentSuperClass_(currentSuperClass)
    , currentClassInterfaces_(currentClassInterfaces)
    , isInterface_(isInterface)
{
    (void)api;
}

BasicValue SimpleVerifier::newValue(const Type* type)
{
    if (type == nullptr) {
        return BasicValue::UNINITIALIZED_VALUE;
    }

    bool isArray = type->getSort() == Type::ARRAY;
    if (isArray) {
        switch (type->getElementType().getSort()) {
        case Type::BOOLEAN:
        case Type::CHAR:
        case Type::BYTE:
        case Type::SHORT: return BasicValue(internType(*type));
        default: break;
        }
    }

    BasicValue value = BasicVerifier::newValue(type);
    if (value.equals(BasicValue::REFERENCE_VALUE)) {
        if (isArray) {
            Type elementType = type->getElementType();
            value            = newValue(internType(elementType));
            ca::str::Utf8StringBuilder descriptor;
            for (int i = 0; i < type->getDimensions(); ++i) {
                descriptor.append(Utf8StringRef::from_cstr("["));
            }
            // #921 staticArena 退役：元素描述符 intern 落点=本 verifier 私有
            // descriptorArena_（与下方 fromArena 同池，随 verifier 回收）。
            descriptor.append(value.getType()->getDescriptor(descriptorArena_));
            auto built = descriptor.build();
            value      = BasicValue(internType(Type::fromArena(descriptorArena_, built.ref())));
        }
        else {
            value = BasicValue(internType(*type));
        }
    }
    return value;
}

bool SimpleVerifier::isArrayValue(const BasicValue& value)
{
    const Type* type = value.getType();
    return type != nullptr &&
           (type->getSort() == Type::ARRAY || *type == BasicInterpreter::NULL_TYPE);
}

BasicValue SimpleVerifier::getElementValue(const BasicValue& objectArrayValue)
{
    const Type* arrayType = objectArrayValue.getType();
    if (arrayType != nullptr) {
        if (arrayType->getSort() == Type::ARRAY) {
            // #921 staticArena 退役：ARRAY 排序 getDescriptor 返回 value_ 不 intern，
            // 落点仍走本 verifier 私有 descriptorArena_。
            auto desc      = arrayType->getDescriptor(descriptorArena_);
            auto innerDesc = desc.slice(1, desc.byte_length());
            return newValue(internType(Type::getType(innerDesc)));
        }
        else if (*arrayType == BasicInterpreter::NULL_TYPE) {
            return objectArrayValue;
        }
    }
    throw AnalyzerException("AssertionError: Invalid array value");
}

bool SimpleVerifier::isSubTypeOf(const BasicValue& value, const BasicValue& expected)
{
    const Type* type         = value.getType();
    const Type* expectedType = expected.getType();

    if (type == nullptr || expectedType == nullptr) {
        return type == nullptr && expectedType == nullptr;
    }
    if (*type == *expectedType) {
        return true;
    }

    switch (expectedType->getSort()) {
    case Type::INT:
    case Type::FLOAT:
    case Type::LONG:
    case Type::DOUBLE: return false;
    case Type::ARRAY:
    case Type::OBJECT:
        if (*type == BasicInterpreter::NULL_TYPE) {
            return true;
        }

        {
            int  dim      = 0;
            Type elemType = *type;
            if (type->getSort() == Type::ARRAY) {
                dim      = type->getDimensions();
                elemType = type->getElementType();
                if (elemType.getSort() != Type::OBJECT) {
                    dim      = dim - 1;
                    elemType = OBJECT_TYPE;
                }
            }

            int  expectedDim  = 0;
            Type expectedElem = *expectedType;
            if (expectedType->getSort() == Type::ARRAY) {
                expectedDim  = expectedType->getDimensions();
                expectedElem = expectedType->getElementType();
                if (expectedElem.getSort() != Type::OBJECT) {
                    return false;
                }
            }

            if (dim < expectedDim) {
                return false;
            }
            if (dim > expectedDim) {
                elemType = OBJECT_TYPE;
            }

            if (isAssignableFrom(expectedElem, elemType)) {
                return true;
            }
            if (isInterface(expectedElem)) {
                return isAssignableFrom(
                    Type::getObjectType(Utf8StringRef::from_cstr("java/lang/Object")), elemType);
            }
            return false;
        }
    default: throw AnalyzerException("AssertionError: Unexpected type sort in isSubTypeOf");
    }
}

BasicValue SimpleVerifier::merge(const BasicValue& value1, const BasicValue& value2)
{
    const Type* type1 = value1.getType();
    const Type* type2 = value2.getType();

    if (type1 == nullptr || type2 == nullptr) {
        return BasicValue::UNINITIALIZED_VALUE;
    }
    if (*type1 == *type2) {
        return value1;
    }
    if (type1->getSort() != Type::OBJECT && type1->getSort() != Type::ARRAY) {
        return BasicValue::UNINITIALIZED_VALUE;
    }
    if (type2->getSort() != Type::OBJECT && type2->getSort() != Type::ARRAY) {
        return BasicValue::UNINITIALIZED_VALUE;
    }
    if (*type1 == BasicInterpreter::NULL_TYPE) {
        return value2;
    }
    if (*type2 == BasicInterpreter::NULL_TYPE) {
        return value1;
    }

    Type t1   = *type1;
    int  dim1 = 0;
    if (t1.getSort() == Type::ARRAY) {
        dim1 = t1.getDimensions();
        t1   = t1.getElementType();
        if (t1.getSort() != Type::OBJECT) {
            dim1 = dim1 - 1;
            t1   = OBJECT_TYPE;
        }
    }

    Type t2   = *type2;
    int  dim2 = 0;
    if (t2.getSort() == Type::ARRAY) {
        dim2 = t2.getDimensions();
        t2   = t2.getElementType();
        if (t2.getSort() != Type::OBJECT) {
            dim2 = dim2 - 1;
            t2   = OBJECT_TYPE;
        }
    }

    if (dim1 != dim2) {
        return newArrayValue(OBJECT_TYPE, std::min(dim1, dim2));
    }
    if (isAssignableFrom(t1, t2)) {
        return newArrayValue(t1, dim1);
    }
    if (isAssignableFrom(t2, t1)) {
        return newArrayValue(t2, dim1);
    }
    if (!isInterface(t1)) {
        while (t1 != OBJECT_TYPE) {
            t1 = getSuperClass(t1);
            if (t1.getSort() == Type::VOID) {
                break;
            }
            if (isAssignableFrom(t1, t2)) {
                return newArrayValue(t1, dim1);
            }
        }
    }
    return newArrayValue(OBJECT_TYPE, dim1);
}

bool SimpleVerifier::isInterface(const Type& type)
{
    if (!isCurrentClassEmpty() && currentClass_ == type) {
        return isInterface_;
    }
    return false;
}

Type SimpleVerifier::getSuperClass(const Type& type)
{
    if (!isCurrentClassEmpty() && currentClass_ == type) {
        return currentSuperClass_;
    }
    return Type{};
}

bool SimpleVerifier::isAssignableFrom(const Type& type1, const Type& type2)
{
    if (type1 == type2) {
        return true;
    }
    if (type2.getSort() == Type::VOID) {
        return false;
    }
    if (!isCurrentClassEmpty() && currentClass_ == type1) {
        Type superType2 = getSuperClass(type2);
        if (superType2.getSort() == Type::VOID) {
            return false;
        }
        if (isInterface_) {
            return type2.getSort() == Type::OBJECT || type2.getSort() == Type::ARRAY;
        }
        return isAssignableFrom(type1, superType2);
    }
    if (!isCurrentClassEmpty() && currentClass_ == type2) {
        if (isAssignableFrom(type1, currentSuperClass_)) {
            return true;
        }
        for (const auto& iface : currentClassInterfaces_) {
            if (isAssignableFrom(type1, iface)) {
                return true;
            }
        }
        return false;
    }
    return type1 == type2;
}

const Type* SimpleVerifier::internType(const Type& type) const
{
    typeCache_.push_back(type);
    return &typeCache_.back();
}

bool SimpleVerifier::isCurrentClassEmpty() const
{
    return currentClass_.getSort() == Type::VOID;
}

BasicValue SimpleVerifier::newArrayValue(const Type& type, int dimensions)
{
    if (dimensions == 0) {
        return newValue(internType(type));
    }
    ca::str::Utf8StringBuilder descriptor;
    for (int i = 0; i < dimensions; ++i) {
        descriptor.append(Utf8StringRef::from_cstr("["));
    }
    // #921 staticArena 退役：数组维数前缀拼接为瞬时使用，intern 落点=本 verifier
    // 私有 descriptorArena_（与下方 fromArena 同池）。
    descriptor.append(type.getDescriptor(descriptorArena_));
    auto built = descriptor.build();
    return newValue(internType(Type::fromArena(descriptorArena_, built.ref())));
}
