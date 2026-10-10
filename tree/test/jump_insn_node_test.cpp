#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(JumpInsnNodeTest, testConstructor)
{
    Label*       label = new Label();
    LabelNode    labelNode(label);
    JumpInsnNode jumpInsnNode(Opcodes::GOTO, label);
    ASSERT_EQ(Opcodes::GOTO, jumpInsnNode.getOpcode());
    ASSERT_EQ(AbstractInsnNode::JUMP_INSN, jumpInsnNode.getType());
    ASSERT_EQ(label, jumpInsnNode.label);
}
