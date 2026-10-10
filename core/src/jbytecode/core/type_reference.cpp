#include "jbytecode/core/type_reference.hpp"
#include <stdexcept>

namespace jbytecode::core {

// ================================================================
// Static factory methods
// ================================================================

TypeReference TypeReference::newTypeReference(int sort)
{
    return TypeReference(sort << 24);
}

TypeReference TypeReference::newTypeParameterReference(int sort, int paramIndex)
{
    return TypeReference((sort << 24) | (paramIndex << 16));
}

TypeReference TypeReference::newTypeParameterBoundReference(int sort, int paramIndex,
                                                            int boundIndex)
{
    return TypeReference((sort << 24) | (paramIndex << 16) | (boundIndex << 8));
}

TypeReference TypeReference::newSuperTypeReference(int itfIndex)
{
    return TypeReference((CLASS_EXTENDS << 24) | ((itfIndex & 0xFFFF) << 8));
}

TypeReference TypeReference::newFormalParameterReference(int paramIndex)
{
    return TypeReference((METHOD_FORMAL_PARAMETER << 24) | (paramIndex << 16));
}

TypeReference TypeReference::newExceptionReference(int exceptionIndex)
{
    return TypeReference((THROWS << 24) | (exceptionIndex << 8));
}

TypeReference TypeReference::newTryCatchReference(int tryCatchBlockIndex)
{
    return TypeReference((EXCEPTION_PARAMETER << 24) | (tryCatchBlockIndex << 8));
}

TypeReference TypeReference::newTypeArgumentReference(int sort, int argIndex)
{
    return TypeReference((sort << 24) | argIndex);
}

// ================================================================
// Instance methods
// ================================================================

int TypeReference::getSort() const
{
    return static_cast<unsigned>(targetTypeAndInfo_) >> 24;
}

int TypeReference::getTypeParameterIndex() const
{
    return (targetTypeAndInfo_ & 0x00FF0000) >> 16;
}

int TypeReference::getTypeParameterBoundIndex() const
{
    return (targetTypeAndInfo_ & 0x0000FF00) >> 8;
}

int TypeReference::getSuperTypeIndex() const
{
    return static_cast<short>((targetTypeAndInfo_ & 0x00FFFF00) >> 8);
}

int TypeReference::getFormalParameterIndex() const
{
    return (targetTypeAndInfo_ & 0x00FF0000) >> 16;
}

int TypeReference::getExceptionIndex() const
{
    return (targetTypeAndInfo_ & 0x00FFFF00) >> 8;
}

int TypeReference::getTryCatchBlockIndex() const
{
    return (targetTypeAndInfo_ & 0x00FFFF00) >> 8;
}

int TypeReference::getTypeArgumentIndex() const
{
    return targetTypeAndInfo_ & 0xFF;
}

int TypeReference::getValue() const
{
    return targetTypeAndInfo_;
}

// ================================================================
// Serialization
// ================================================================

void TypeReference::putTarget(int targetTypeAndInfo, ByteVector& output)
{
    switch (static_cast<unsigned>(targetTypeAndInfo) >> 24) {
    case CLASS_TYPE_PARAMETER:
    case METHOD_TYPE_PARAMETER:
    case METHOD_FORMAL_PARAMETER: output.putShort(targetTypeAndInfo >> 16); break;
    case FIELD:
    case METHOD_RETURN:
    case METHOD_RECEIVER: output.putByte(targetTypeAndInfo >> 24); break;
    case CAST:
    case CONSTRUCTOR_INVOCATION_TYPE_ARGUMENT:
    case METHOD_INVOCATION_TYPE_ARGUMENT:
    case CONSTRUCTOR_REFERENCE_TYPE_ARGUMENT:
    case METHOD_REFERENCE_TYPE_ARGUMENT: output.putInt(targetTypeAndInfo); break;
    case CLASS_EXTENDS:
    case CLASS_TYPE_PARAMETER_BOUND:
    case METHOD_TYPE_PARAMETER_BOUND:
    case THROWS:
    case EXCEPTION_PARAMETER:
    case INSTANCEOF:
    case NEW:
    case CONSTRUCTOR_REFERENCE:
    case METHOD_REFERENCE:
        output.put12(targetTypeAndInfo >> 24, (targetTypeAndInfo & 0xFFFF00) >> 8);
        break;
    default: throw std::invalid_argument("Unsupported target type");
    }
}

}   // namespace jbytecode::core
