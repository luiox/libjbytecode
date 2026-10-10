#pragma once

#include "jbytecode/core/byte_vector.hpp"

namespace jbytecode::core {

/// @brief A reference to a type appearing in a class, field or method declaration, or on an
/// instruction. Such a reference designates the part of the class where the referenced type is
/// appearing (e.g. an 'extends', 'implements' or 'throws' clause, a 'new' instruction, a 'catch'
/// clause, a type cast, a local variable declaration, etc).
///
/// The target_type and target_info structures (JVMS 4.7.20.1) are encoded in a single int:
///   - The most significant byte stores the sort (target_type).
///   - The remaining bytes store target_info fields (parameter index, bound index, etc.).
///
/// @see <a href="https://docs.oracle.com/javase/specs/jvms/se9/html/jvms-4.html#jvms-4.7.20">JVMS
///     4.7.20</a>
class TypeReference
{
public:
    // ================================================================
    // Sort constants (target_type values from JVMS Table 4.7.20-A)
    // ================================================================

    /// Type parameter of a generic class.
    static constexpr int CLASS_TYPE_PARAMETER = 0x00;
    /// Type parameter of a generic method.
    static constexpr int METHOD_TYPE_PARAMETER = 0x01;
    /// Super class or an interface it implements.
    static constexpr int CLASS_EXTENDS = 0x10;
    /// Bound of a type parameter of a generic class.
    static constexpr int CLASS_TYPE_PARAMETER_BOUND = 0x11;
    /// Bound of a type parameter of a generic method.
    static constexpr int METHOD_TYPE_PARAMETER_BOUND = 0x12;
    /// Type of a field.
    static constexpr int FIELD = 0x13;
    /// Return type of a method.
    static constexpr int METHOD_RETURN = 0x14;
    /// Receiver type of a method.
    static constexpr int METHOD_RECEIVER = 0x15;
    /// Type of a formal parameter of a method.
    static constexpr int METHOD_FORMAL_PARAMETER = 0x16;
    /// Type of an exception declared in the 'throws' clause.
    static constexpr int THROWS = 0x17;
    /// Type of a local variable in a method.
    static constexpr int LOCAL_VARIABLE = 0x40;
    /// Type of a resource variable in a method.
    static constexpr int RESOURCE_VARIABLE = 0x41;
    /// Type of the exception of a 'catch' clause.
    static constexpr int EXCEPTION_PARAMETER = 0x42;
    /// Type in an 'instanceof' instruction.
    static constexpr int INSTANCEOF = 0x43;
    /// Type of the object created by a 'new' instruction.
    static constexpr int NEW = 0x44;
    /// Receiver type of a constructor reference.
    static constexpr int CONSTRUCTOR_REFERENCE = 0x45;
    /// Receiver type of a method reference.
    static constexpr int METHOD_REFERENCE = 0x46;
    /// Type in a cast instruction.
    static constexpr int CAST = 0x47;
    /// Type argument in a constructor invocation.
    static constexpr int CONSTRUCTOR_INVOCATION_TYPE_ARGUMENT = 0x48;
    /// Type argument in a method invocation.
    static constexpr int METHOD_INVOCATION_TYPE_ARGUMENT = 0x49;
    /// Type argument in a constructor reference.
    static constexpr int CONSTRUCTOR_REFERENCE_TYPE_ARGUMENT = 0x4A;
    /// Type argument in a method reference.
    static constexpr int METHOD_REFERENCE_TYPE_ARGUMENT = 0x4B;

    // ================================================================
    // Construction
    // ================================================================

    /// Constructs a TypeReference from its int encoded value.
    /// @param typeRef the encoded value, as received in visitTypeAnnotation.
    explicit TypeReference(int typeRef)
        : targetTypeAndInfo_(typeRef)
    {}

    // ================================================================
    // Static factory methods
    // ================================================================

    /// Returns a type reference of the given simple sort (FIELD, METHOD_RETURN, etc.).
    static TypeReference newTypeReference(int sort);

    /// Returns a reference to a type parameter of a generic class or method.
    static TypeReference newTypeParameterReference(int sort, int paramIndex);

    /// Returns a reference to a type parameter bound.
    static TypeReference newTypeParameterBoundReference(int sort, int paramIndex, int boundIndex);

    /// Returns a reference to the super class or an interface (itfIndex = -1 for super class).
    static TypeReference newSuperTypeReference(int itfIndex);

    /// Returns a reference to the type of a formal parameter.
    static TypeReference newFormalParameterReference(int paramIndex);

    /// Returns a reference to the type of an exception in a 'throws' clause.
    static TypeReference newExceptionReference(int exceptionIndex);

    /// Returns a reference to the type of a 'catch' clause exception.
    static TypeReference newTryCatchReference(int tryCatchBlockIndex);

    /// Returns a reference to a type argument (CAST, etc.).
    static TypeReference newTypeArgumentReference(int sort, int argIndex);

    // ================================================================
    // Instance methods
    // ================================================================

    /// Returns the sort of this type reference.
    int getSort() const;

    /// Returns the index of the type parameter (for CLASS/METHOD_TYPE_PARAMETER* sorts).
    int getTypeParameterIndex() const;

    /// Returns the index of the type parameter bound.
    int getTypeParameterBoundIndex() const;

    /// Returns the index of the "super type" (for CLASS_EXTENDS sort).
    /// @return the interface index, or -1 if it references the super class.
    int getSuperTypeIndex() const;

    /// Returns the formal parameter index (for METHOD_FORMAL_PARAMETER).
    int getFormalParameterIndex() const;

    /// Returns the exception index (for THROWS).
    int getExceptionIndex() const;

    /// Returns the try catch block index (for EXCEPTION_PARAMETER).
    int getTryCatchBlockIndex() const;

    /// Returns the type argument index (for CAST, etc.).
    int getTypeArgumentIndex() const;

    /// Returns the int encoded value of this type reference.
    int getValue() const;

    // ================================================================
    // Serialization
    // ================================================================

    /// Puts the target_type and target_info into the ByteVector, encoded according to JVMS 4.7.20.1.
    /// LOCAL_VARIABLE and RESOURCE_VARIABLE target types are not supported (use
    /// visitLocalVariableAnnotation directly).
    static void putTarget(int targetTypeAndInfo, ByteVector& output);

private:
    int targetTypeAndInfo_;
};

}   // namespace jbytecode::core
