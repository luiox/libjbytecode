#include <gtest/gtest.h>
#include "jbytecode/core/type_reference.hpp"

using namespace jbytecode::core;

TEST(TypeReferenceTest, testNewTypeReference)
{
    TypeReference typeReference = TypeReference::newTypeReference(TypeReference::FIELD);

    ASSERT_EQ(TypeReference::FIELD, typeReference.getSort());
    ASSERT_EQ(TypeReference::FIELD << 24, typeReference.getValue());
}

TEST(TypeReferenceTest, testNewTypeParameterReference)
{
    TypeReference typeReference =
        TypeReference::newTypeParameterReference(TypeReference::CLASS_TYPE_PARAMETER, 3);

    ASSERT_EQ(TypeReference::CLASS_TYPE_PARAMETER, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getTypeParameterIndex());
}

TEST(TypeReferenceTest, testNewTypeParameterBoundReference)
{
    TypeReference typeReference =
        TypeReference::newTypeParameterBoundReference(TypeReference::CLASS_TYPE_PARAMETER, 3, 7);

    ASSERT_EQ(TypeReference::CLASS_TYPE_PARAMETER, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getTypeParameterIndex());
    ASSERT_EQ(7, typeReference.getTypeParameterBoundIndex());
}

TEST(TypeReferenceTest, testNewSuperTypeReference)
{
    TypeReference typeReference = TypeReference::newSuperTypeReference(-1);

    ASSERT_EQ(TypeReference::CLASS_EXTENDS, typeReference.getSort());
    ASSERT_EQ(-1, typeReference.getSuperTypeIndex());
}

TEST(TypeReferenceTest, testNewFormalParameterReference)
{
    TypeReference typeReference = TypeReference::newFormalParameterReference(3);

    ASSERT_EQ(TypeReference::METHOD_FORMAL_PARAMETER, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getFormalParameterIndex());
}

TEST(TypeReferenceTest, testNewExceptionReference)
{
    TypeReference typeReference = TypeReference::newExceptionReference(3);

    ASSERT_EQ(TypeReference::THROWS, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getExceptionIndex());
}

TEST(TypeReferenceTest, testNewTryCatchReference)
{
    TypeReference typeReference = TypeReference::newTryCatchReference(3);

    ASSERT_EQ(TypeReference::EXCEPTION_PARAMETER, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getTryCatchBlockIndex());
}

TEST(TypeReferenceTest, testNewTypeArgumentReference)
{
    TypeReference typeReference = TypeReference::newTypeArgumentReference(TypeReference::CAST, 3);

    ASSERT_EQ(TypeReference::CAST, typeReference.getSort());
    ASSERT_EQ(3, typeReference.getTypeArgumentIndex());
}
