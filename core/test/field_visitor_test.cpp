#include <gtest/gtest.h>
#include "jbytecode/core/visitors.hpp"

using namespace jbytecode::core;

TEST(FieldVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ FieldVisitor fv(Opcodes::ASM4); });
}

TEST(FieldVisitorTest, testConstructor_invalidApi)
{
    try {
        FieldVisitor fv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(FieldVisitorTest, testGetDelegate)
{
    FieldVisitor* delegate = new FieldVisitor(Opcodes::ASM4);
    FieldVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}
