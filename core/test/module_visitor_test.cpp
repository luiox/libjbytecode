#include <gtest/gtest.h>
#include "jbytecode/core/visitors.hpp"

using namespace jbytecode::core;

TEST(ModuleVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ ModuleVisitor mv(Opcodes::ASM4); });
}

TEST(ModuleVisitorTest, testConstructor_invalidApi)
{
    try {
        ModuleVisitor mv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(ModuleVisitorTest, testGetDelegate)
{
    ModuleVisitor* delegate = new ModuleVisitor(Opcodes::ASM4);
    ModuleVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}
