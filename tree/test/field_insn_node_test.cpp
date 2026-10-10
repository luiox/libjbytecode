#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(FieldInsnNodeTest, testConstructor)
{
    FieldInsnNode fieldInsnNode(Opcodes::GETSTATIC,
                                ZUtf8StringRef::from_static("owner"),
                                ZUtf8StringRef::from_static("name"),
                                ZUtf8StringRef::from_static("I"));
    ASSERT_EQ(AbstractInsnNode::FIELD_INSN, fieldInsnNode.getType());
    ASSERT_EQ(Opcodes::GETSTATIC, fieldInsnNode.getOpcode());
    ASSERT_EQ(ZUtf8StringRef::from_static("owner"), fieldInsnNode.owner);
    ASSERT_EQ(ZUtf8StringRef::from_static("name"), fieldInsnNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("I"), fieldInsnNode.desc);
}
