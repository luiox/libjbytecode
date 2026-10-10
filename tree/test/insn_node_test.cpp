#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(InsnNodeTest, testConstructor)
{
    InsnNode insnNode(Opcodes::ACONST_NULL);
    ASSERT_EQ(AbstractInsnNode::INSN, insnNode.getType());
    ASSERT_EQ(Opcodes::ACONST_NULL, insnNode.getOpcode());
}
