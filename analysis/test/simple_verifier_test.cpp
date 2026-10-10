#include <gtest/gtest.h>
#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/simple_verifier.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;
using namespace ca::str;

// SimpleVerifier 仅 newValue 为 public override，merge/isArrayValue 等为
// protected（经 Analyzer 流程间接生效，见 analyzer_test）。这里覆盖公开面。

TEST(SimpleVerifierTest, newValueNullIsUninitialized)
{
    SimpleVerifier v;
    EXPECT_EQ(BasicValue::UNINITIALIZED_VALUE, v.newValue(nullptr));
}

TEST(SimpleVerifierTest, newValuePrimitiveInt)
{
    SimpleVerifier v;
    EXPECT_EQ(BasicValue::INT_VALUE, v.newValue(&Type::INT_TYPE));
}

TEST(SimpleVerifierTest, newValuePrimitiveLongIsTwoWord)
{
    SimpleVerifier v;
    BasicValue     val = v.newValue(&Type::LONG_TYPE);
    EXPECT_EQ(BasicValue::LONG_VALUE, val);
    EXPECT_EQ(2, val.getSize());
}

TEST(SimpleVerifierTest, newValueObjectIsReference)
{
    SimpleVerifier v;
    Type           obj = Type::getObjectType(ZUtf8StringRef::from_static("java/lang/String"));
    BasicValue     val = v.newValue(&obj);
    EXPECT_TRUE(val.isReference());
}

TEST(SimpleVerifierTest, newValueArrayIsReference)
{
    SimpleVerifier v;
    Type           arr = Type::getType(ZUtf8StringRef::from_static("[I"));
    BasicValue     val = v.newValue(&arr);
    EXPECT_TRUE(val.isReference());
}
