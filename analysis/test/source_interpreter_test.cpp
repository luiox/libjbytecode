#include <gtest/gtest.h>
#include <string>

#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/source_interpreter.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;
using namespace jbytecode::analysis;

TEST(SourceValueTest, defaultSize)
{
    SourceValue v;
    EXPECT_EQ(v.getSize(), 0);
}

TEST(SourceValueTest, sizeOne)
{
    SourceValue v(1);
    EXPECT_EQ(v.getSize(), 1);
}

TEST(SourceValueTest, equality)
{
    SourceValue a(1);
    SourceValue b(1);
    SourceValue c(2);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(SourceValueTest, withInstruction)
{
    InsnNode    insn(Opcodes::ICONST_0);
    SourceValue v(1, &insn);
    EXPECT_EQ(v.getSize(), 1);
    EXPECT_EQ(v.insns_.size(), 1u);
}

TEST(SourceInterpreterTest, newValueInt)
{
    SourceInterpreter interp;
    SourceValue       v = interp.newValue(&Type::INT_TYPE);
    EXPECT_EQ(v.getSize(), 1);
}

TEST(SourceInterpreterTest, newValueLong)
{
    SourceInterpreter interp;
    SourceValue       v = interp.newValue(&Type::LONG_TYPE);
    EXPECT_EQ(v.getSize(), 2);
}

TEST(SourceInterpreterTest, newValueNullReturnsSize1)
{
    SourceInterpreter interp;
    SourceValue       v = interp.newValue(nullptr);
    EXPECT_EQ(v.getSize(), 1);
}

TEST(SourceInterpreterTest, newValueVoidReturnsEmpty)
{
    SourceInterpreter interp;
    SourceValue       v = interp.newValue(&Type::VOID_TYPE);
    EXPECT_EQ(v.getSize(), 0);
}

TEST(SourceInterpreterTest, newOperationUnary)
{
    SourceInterpreter interp;
    InsnNode          ineg(Opcodes::INEG);
    SourceValue       v = interp.newOperation(&ineg);
    EXPECT_EQ(v.getSize(), 1);
}

TEST(SourceInterpreterTest, copyOperation)
{
    SourceInterpreter interp;
    InsnNode          insn(Opcodes::NOP);
    SourceValue       src(2);
    SourceValue       result = interp.copyOperation(&insn, src);
    EXPECT_EQ(result.getSize(), 2);
}

TEST(SourceInterpreterTest, mergeValues)
{
    SourceInterpreter interp;
    InsnNode          a(Opcodes::ICONST_0);
    InsnNode          b(Opcodes::ICONST_1);
    SourceValue       va(1, &a);
    SourceValue       vb(1, &b);
    SourceValue       merged = interp.merge(va, vb);
    EXPECT_EQ(merged.getSize(), 1);
    EXPECT_EQ(merged.insns_.size(), 2u);
}

TEST(SourceInterpreterTest, mergeSameValueReturnsOriginal)
{
    SourceInterpreter interp;
    InsnNode          a(Opcodes::ICONST_0);
    SourceValue       va(1, &a);
    SourceValue       merged = interp.merge(va, va);
    EXPECT_EQ(merged.getSize(), 1);
}
