#include <gtest/gtest.h>
#include "test_utils.hpp"


TEST(InvokeDynamicInsnNodeTest, testConstructor)
{
    Handle                     handle(Opcodes::H_INVOKESTATIC,
                  ZUtf8StringRef::from_static("owner"),
                  ZUtf8StringRef::from_static("name"),
                  ZUtf8StringRef::from_static("()V"),
                  false);
    std::vector<ca::core::Any> bsmArgs = {std::string("s")};
    InvokeDynamicInsnNode      invokeDynamicInsnNode(
        ZUtf8StringRef::from_static("name"), ZUtf8StringRef::from_static("()V"), handle, bsmArgs);
    ASSERT_EQ(Opcodes::INVOKEDYNAMIC, invokeDynamicInsnNode.getOpcode());
    ASSERT_EQ(AbstractInsnNode::INVOKE_DYNAMIC_INSN, invokeDynamicInsnNode.getType());
    ASSERT_EQ(ZUtf8StringRef::from_static("name"), invokeDynamicInsnNode.name);
    ASSERT_EQ(ZUtf8StringRef::from_static("()V"), invokeDynamicInsnNode.desc);
}
