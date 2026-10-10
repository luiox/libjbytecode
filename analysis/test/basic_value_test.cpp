#include <gtest/gtest.h>
#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/basic_interpreter.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;

TEST(BasicValueTest, sizeOfSingleWordValues)
{
    EXPECT_EQ(1, BasicValue::INT_VALUE.getSize());
    EXPECT_EQ(1, BasicValue::FLOAT_VALUE.getSize());
    EXPECT_EQ(1, BasicValue::REFERENCE_VALUE.getSize());
    EXPECT_EQ(1, BasicValue::UNINITIALIZED_VALUE.getSize());
}

TEST(BasicValueTest, sizeOfDoubleWordValues)
{
    EXPECT_EQ(2, BasicValue::LONG_VALUE.getSize());
    EXPECT_EQ(2, BasicValue::DOUBLE_VALUE.getSize());
}

TEST(BasicValueTest, isReference)
{
    EXPECT_TRUE(BasicValue::REFERENCE_VALUE.isReference());
    EXPECT_FALSE(BasicValue::INT_VALUE.isReference());
    EXPECT_FALSE(BasicValue::LONG_VALUE.isReference());
    // 无类型（UNINITIALIZED）不是引用
    EXPECT_FALSE(BasicValue::UNINITIALIZED_VALUE.isReference());
}

TEST(BasicValueTest, equalsSameType)
{
    EXPECT_EQ(BasicValue::INT_VALUE, BasicValue::INT_VALUE);
    EXPECT_TRUE(BasicValue::INT_VALUE.equals(BasicValue::INT_VALUE));
}

TEST(BasicValueTest, equalsDifferentType)
{
    EXPECT_NE(BasicValue::INT_VALUE, BasicValue::FLOAT_VALUE);
    EXPECT_NE(BasicValue::LONG_VALUE, BasicValue::DOUBLE_VALUE);
}

TEST(BasicValueTest, uninitializedEqualsUninitialized)
{
    BasicValue a;
    BasicValue b;
    EXPECT_EQ(a, b);
    // 无类型 vs 有类型不相等
    EXPECT_NE(a, BasicValue::INT_VALUE);
    EXPECT_NE(BasicValue::INT_VALUE, a);
}

TEST(BasicValueTest, getTypeRoundTrip)
{
    EXPECT_EQ(&Type::INT_TYPE, BasicValue::INT_VALUE.getType());
    EXPECT_EQ(nullptr, BasicValue::UNINITIALIZED_VALUE.getType());
}

TEST(BasicValueTest, toStringSpecialValues)
{
    EXPECT_EQ(".", BasicValue::UNINITIALIZED_VALUE.toString());
    EXPECT_EQ("A", BasicValue::RETURNADDRESS_VALUE.toString());
    EXPECT_EQ("R", BasicValue::REFERENCE_VALUE.toString());
}
