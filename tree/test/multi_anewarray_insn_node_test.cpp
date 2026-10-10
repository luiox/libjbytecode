#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(MultiANewArrayInsnNodeTest, testConstructor)
{
    MultiANewArrayInsnNode multiANewArrayInsnNode(ZUtf8StringRef::from_static("[[I"), 2);
    ASSERT_EQ(AbstractInsnNode::MULTIANEWARRAY_INSN, multiANewArrayInsnNode.getType());
    ASSERT_EQ(ZUtf8StringRef::from_static("[[I"), multiANewArrayInsnNode.desc);
    ASSERT_EQ(2, multiANewArrayInsnNode.dims);
}
