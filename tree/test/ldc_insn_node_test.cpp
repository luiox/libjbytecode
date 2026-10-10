#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(LdcInsnNodeTest, testConstructor)
{
    LdcInsnNode ldcInsnNode(std::string("s"));
    ASSERT_EQ(AbstractInsnNode::LDC_INSN, ldcInsnNode.getType());
    ASSERT_EQ("s", ldcInsnNode.cst.cast<std::string>());
}

// 回归 #742 补充：真实编译管线里 STRING 常量经 libjblw conversion.cpp
// unwrapConstant 以 Utf8StringRef 标签落入 LdcInsnNode（而非 std::string）。
// 原构造测试只覆盖了 std::string 标签，绕开了真实管线，故补一条 Utf8StringRef
// 标签的构造用例。
TEST(LdcInsnNodeTest, testConstructorWithUtf8StringRefFromPipeline)
{
    LdcInsnNode ldcInsnNode(Utf8StringRef::from_cstr("hi"));
    ASSERT_EQ(AbstractInsnNode::LDC_INSN, ldcInsnNode.getType());
    ASSERT_EQ("hi", ldcInsnNode.cst.cast<Utf8StringRef>());
}
