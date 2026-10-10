#include <gtest/gtest.h>
#include "test_utils.hpp"

// CodeSizeEvaluator 计算方法体编码后的字节大小区间 [minSize, maxSize]。
// 多数指令 min==max；只有跳转因短/宽形式可能 min<max。
// 构造传 nullptr 委托：仅测量，不转发。

TEST(CodeSizeEvaluatorTest, simpleInsnIsOneByte)
{
    CodeSizeEvaluator e(nullptr);
    e.visitInsn(Opcodes::NOP);
    e.visitInsn(Opcodes::IADD);
    EXPECT_EQ(2, e.getMinSize());
    EXPECT_EQ(2, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, varInsnLowIndexIsOneByte)
{
    CodeSizeEvaluator e(nullptr);
    e.visitVarInsn(Opcodes::ILOAD, 0);   // index<4 → 1 字节
    EXPECT_EQ(1, e.getMinSize());
    EXPECT_EQ(1, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, varInsnMidIndexIsTwoBytes)
{
    CodeSizeEvaluator e(nullptr);
    e.visitVarInsn(Opcodes::ILOAD, 10);   // 4<=index<256 → 2 字节
    EXPECT_EQ(2, e.getMinSize());
    EXPECT_EQ(2, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, varInsnWideIndexIsFourBytes)
{
    CodeSizeEvaluator e(nullptr);
    e.visitVarInsn(Opcodes::ILOAD, 300);   // index>=256 → wide → 4 字节
    EXPECT_EQ(4, e.getMinSize());
    EXPECT_EQ(4, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, sipushIsThreeBytesBipushTwo)
{
    CodeSizeEvaluator e(nullptr);
    e.visitIntInsn(Opcodes::BIPUSH, 1);   // 2
    e.visitIntInsn(Opcodes::SIPUSH, 1);   // 3
    EXPECT_EQ(5, e.getMinSize());
    EXPECT_EQ(5, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, conditionalJumpHasWiderMax)
{
    CodeSizeEvaluator e(nullptr);
    Label             l;
    e.visitJumpInsn(Opcodes::IFEQ, &l);   // min 3, max 8（条件跳转可能扩展）
    EXPECT_EQ(3, e.getMinSize());
    EXPECT_EQ(8, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, gotoMaxIsFive)
{
    CodeSizeEvaluator e(nullptr);
    Label             l;
    e.visitJumpInsn(Opcodes::GOTO, &l);   // min 3, max 5
    EXPECT_EQ(3, e.getMinSize());
    EXPECT_EQ(5, e.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, iincNarrowVsWide)
{
    CodeSizeEvaluator narrow(nullptr);
    narrow.visitIincInsn(5, 1);   // 3 字节
    EXPECT_EQ(3, narrow.getMinSize());

    CodeSizeEvaluator wide(nullptr);
    wide.visitIincInsn(300, 1);   // varIndex>255 → wide → 6 字节
    EXPECT_EQ(6, wide.getMinSize());
    EXPECT_EQ(6, wide.getMaxSize());
}

TEST(CodeSizeEvaluatorTest, accumulatesAcrossInstructions)
{
    CodeSizeEvaluator e(nullptr);
    e.visitInsn(Opcodes::ICONST_0);       // 1
    e.visitVarInsn(Opcodes::ISTORE, 1);   // 1 (index<4)
    e.visitInsn(Opcodes::RETURN);         // 1
    EXPECT_EQ(3, e.getMinSize());
    EXPECT_EQ(3, e.getMaxSize());
}
