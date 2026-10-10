#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(TypeInsnNodeTest, testConstructor)
{
    TypeInsnNode typeInsnNode(Opcodes::NEW, ZUtf8StringRef::from_static("java/lang/Object"));
    ASSERT_EQ(Opcodes::NEW, typeInsnNode.getOpcode());
    ASSERT_EQ(AbstractInsnNode::TYPE_INSN, typeInsnNode.getType());
    ASSERT_EQ(ZUtf8StringRef::from_static("java/lang/Object"), typeInsnNode.desc);
}
