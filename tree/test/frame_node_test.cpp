#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(FrameNodeTest, testConstructor)
{
    std::vector<ca::core::Any> locals = {std::string("l")};
    std::vector<ca::core::Any> stack  = {std::string("s"), std::string("t")};
    FrameNode                  frameNode(Opcodes::F_FULL, locals, stack);
    ASSERT_EQ(AbstractInsnNode::FRAME, frameNode.getType());
    ASSERT_EQ(Opcodes::F_FULL, frameNode.frameType);
    ASSERT_EQ(1u, frameNode.local.size());
    ASSERT_EQ(2u, frameNode.stack.size());
}
