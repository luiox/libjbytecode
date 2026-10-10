#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(VarInsnNodeTest, testConstructor)
{
    VarInsnNode varInsnNode(Opcodes::ALOAD, 123);
    ASSERT_EQ(Opcodes::ALOAD, varInsnNode.getOpcode());
    ASSERT_EQ(AbstractInsnNode::VAR_INSN, varInsnNode.getType());
    ASSERT_EQ(123, varInsnNode.var);
}
