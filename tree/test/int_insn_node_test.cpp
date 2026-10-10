#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(IntInsnNodeTest, testConstructor)
{
    IntInsnNode intInsnNode(Opcodes::BIPUSH, 0);
    ASSERT_EQ(Opcodes::BIPUSH, intInsnNode.getOpcode());
    ASSERT_EQ(AbstractInsnNode::INT_INSN, intInsnNode.getType());
}
