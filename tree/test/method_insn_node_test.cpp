#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(MethodInsnNodeTest, testConstructor)
{
    MethodInsnNode methodInsnNode(Opcodes::INVOKESTATIC,
                                  ZUtf8StringRef::from_static("owner"),
                                  ZUtf8StringRef::from_static("name"),
                                  ZUtf8StringRef::from_static("()I"),
                                  false);
    ASSERT_EQ(AbstractInsnNode::METHOD_INSN, methodInsnNode.getType());
    ASSERT_EQ(Opcodes::INVOKESTATIC, methodInsnNode.getOpcode());
    ASSERT_EQ(ZUtf8StringRef::from_static("owner"), methodInsnNode.owner);
    ASSERT_EQ(ZUtf8StringRef::from_static("name"), methodInsnNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("()I"), methodInsnNode.desc);
    ASSERT_FALSE(methodInsnNode.isInterface);
}
