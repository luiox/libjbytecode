#include <gtest/gtest.h>
#include "test_utils.hpp"
#include "jbytecode/util/check_adapters.hpp"

TEST(CheckMethodAdapterTest, testConstructor)
{
    jbytecode::util::CheckMethodAdapter cma(nullptr);
    SUCCEED();
}

TEST(CheckMethodAdapterTest, testInsnRequiresVisitCode)
{
    jbytecode::util::CheckMethodAdapter cma(nullptr);
    EXPECT_THROW(cma.visitInsn(Opcodes::NOP), std::exception);
}

TEST(CheckMethodAdapterTest, testInsnAfterVisitCode)
{
    jbytecode::util::CheckMethodAdapter cma(nullptr);
    cma.visitCode();
    EXPECT_NO_THROW(cma.visitInsn(Opcodes::NOP));
    EXPECT_NO_THROW(cma.visitInsn(Opcodes::IADD));
    EXPECT_NO_THROW(cma.visitInsn(Opcodes::RETURN));
}
