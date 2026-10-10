#include <gtest/gtest.h>
#include "jbytecode/core/visitors.hpp"

using namespace jbytecode::core;

TEST(RecordComponentVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ RecordComponentVisitor rcv(Opcodes::ASM4); });
}

TEST(RecordComponentVisitorTest, testConstructor_invalidApi)
{
    try {
        RecordComponentVisitor rcv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(RecordComponentVisitorTest, testGetDelegate)
{
    RecordComponentVisitor* delegate = new RecordComponentVisitor(Opcodes::ASM4);
    RecordComponentVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}
