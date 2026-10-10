#include <gtest/gtest.h>
#include "jbytecode/core/primitives.hpp"
#include "test_utils.hpp"

using namespace jbytecode::core;

TEST(TypePathTest, testGetLength)
{
    ASSERT_EQ(5, TypePath::fromString(ZUtf8StringRef::from_static("[.[*0")).getLength());
    ASSERT_EQ(5, TypePath::fromString(ZUtf8StringRef::from_static("[*0;*[")).getLength());
    ASSERT_EQ(1, TypePath::fromString(ZUtf8StringRef::from_static("10;")).getLength());
    ASSERT_EQ(2, TypePath::fromString(ZUtf8StringRef::from_static("1;0;")).getLength());
}

TEST(TypePathTest, testGetStep)
{
    TypePath typePath = TypePath::fromString(ZUtf8StringRef::from_static("[.[*7"));

    ASSERT_EQ(TypePath::ARRAY_ELEMENT, typePath.getStep(0));
    ASSERT_EQ(TypePath::INNER_TYPE, typePath.getStep(1));
    ASSERT_EQ(TypePath::WILDCARD_BOUND, typePath.getStep(3));
    ASSERT_EQ(TypePath::TYPE_ARGUMENT, typePath.getStep(4));
    ASSERT_EQ(7, typePath.getStepArgument(4));
}

TEST(TypePathTest, testFromAndToString)
{
    ASSERT_EQ("", TypePath::fromString(ZUtf8StringRef::from_static("")).toString());
    ASSERT_EQ("", TypePath::fromString(ZUtf8StringRef::from_static("")).toString());
    ASSERT_EQ("[.[*0;", TypePath::fromString(ZUtf8StringRef::from_static("[.[*0")).toString());
    ASSERT_EQ("[*0;*[", TypePath::fromString(ZUtf8StringRef::from_static("[*0;*[")).toString());
    ASSERT_EQ("10;", TypePath::fromString(ZUtf8StringRef::from_static("10;")).toString());
    ASSERT_EQ("1;0;", TypePath::fromString(ZUtf8StringRef::from_static("1;0;")).toString());
    ASSERT_THROW(TypePath::fromString(ZUtf8StringRef::from_static("-")), std::invalid_argument);
    ASSERT_THROW(TypePath::fromString(ZUtf8StringRef::from_static("=")), std::invalid_argument);
    ASSERT_THROW(TypePath::fromString(ZUtf8StringRef::from_static("1-")), std::invalid_argument);
}
