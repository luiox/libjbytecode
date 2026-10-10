#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(IincInsnNodeTest, testConstructor)
{
    IincInsnNode iincInsnNode(1, 2);
    ASSERT_EQ(AbstractInsnNode::IINC_INSN, iincInsnNode.getType());
    ASSERT_EQ(1, iincInsnNode.var);
    ASSERT_EQ(2, iincInsnNode.incr);
}
