#include <gtest/gtest.h>

#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/source_interpreter.hpp"
#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;

// 与 source_interpreter_test 的 SourceValueTest 互补：
// 这里覆盖 SmallSet 构造、hash()、operator!= 的边界。

TEST(SourceValueExtraTest, defaultIsEmptyZeroSize)
{
    SourceValue v;
    EXPECT_EQ(0, v.getSize());
    EXPECT_EQ(0u, v.insns_.size());
}

TEST(SourceValueExtraTest, constructFromInsnSet)
{
    InsnNode                          a(Opcodes::NOP);
    InsnNode                          b(Opcodes::ICONST_0);
    SmallSet<const AbstractInsnNode*> set(&a, &b);
    SourceValue                       v(1, set);
    EXPECT_EQ(1, v.getSize());
    EXPECT_EQ(2u, v.insns_.size());
    EXPECT_TRUE(v.insns_.contains(&a));
    EXPECT_TRUE(v.insns_.contains(&b));
}

TEST(SourceValueExtraTest, inequalityOnDifferentSize)
{
    InsnNode    a(Opcodes::NOP);
    SourceValue v1(1, &a);
    SourceValue v2(2, &a);
    EXPECT_NE(v1, v2);
}

TEST(SourceValueExtraTest, inequalityOnDifferentInsns)
{
    InsnNode    a(Opcodes::NOP);
    InsnNode    b(Opcodes::ICONST_0);
    SourceValue v1(1, &a);
    SourceValue v2(1, &b);
    EXPECT_NE(v1, v2);
}

TEST(SourceValueExtraTest, hashStableAndEqualForEqualValues)
{
    InsnNode    a(Opcodes::NOP);
    SourceValue v1(1, &a);
    SourceValue v2(1, &a);
    EXPECT_EQ(v1, v2);
    EXPECT_EQ(v1.hash(), v2.hash());
    // 同一对象多次调用稳定
    EXPECT_EQ(v1.hash(), v1.hash());
}
