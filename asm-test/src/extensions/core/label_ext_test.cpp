#include <gtest/gtest.h>
#include "jbytecode/core/primitives.hpp"

using namespace jbytecode::core;

// ---- Edge Tests ----

TEST(EdgeTest, constructJump)
{
    Label target;
    Edge  edge(Edge::JUMP, &target, nullptr);
    ASSERT_EQ(Edge::JUMP, edge.info);
    ASSERT_EQ(&target, edge.successor);
    ASSERT_EQ(nullptr, edge.nextEdge);
}

TEST(EdgeTest, constructException)
{
    Label target;
    Edge  edge(Edge::EXCEPTION, &target, nullptr);
    ASSERT_EQ(Edge::EXCEPTION, edge.info);
}

TEST(EdgeTest, chained)
{
    Label t1, t2;
    Edge  e2(Edge::JUMP, &t2, nullptr);
    Edge  e1(Edge::JUMP, &t1, &e2);
    ASSERT_EQ(&e2, e1.nextEdge);
    ASSERT_EQ(nullptr, e2.nextEdge);
}

// ---- Label Flags Tests ----

TEST(LabelInternalTest, defaultFlags)
{
    Label label;
    ASSERT_FALSE(label.isJumpTarget());
    ASSERT_FALSE(label.isDebugOnly());
    ASSERT_FALSE(label.isResolved());
    ASSERT_FALSE(label.isReachable());
    ASSERT_FALSE(label.isSubroutine());
}

TEST(LabelInternalTest, setFlags)
{
    Label label;
    label.setJumpTarget(true);
    ASSERT_TRUE(label.isJumpTarget());
    label.setJumpTarget(false);
    ASSERT_FALSE(label.isJumpTarget());
}

TEST(LabelInternalTest, debugOnlyFlag)
{
    Label label;
    label.setDebugOnly(true);
    ASSERT_TRUE(label.isDebugOnly());
    // debug-only is independent from reachability.
    ASSERT_FALSE(label.isReachable());
}

TEST(LabelInternalTest, subroutineFlags)
{
    Label label;
    ASSERT_FALSE(label.isSubroutine());
    label.setSubroutineStart(true);
    ASSERT_TRUE(label.isSubroutineStart());
    ASSERT_TRUE(label.isSubroutine());
    label.setSubroutineEnd(true);
    ASSERT_TRUE(label.isSubroutineEnd());
}

TEST(LabelInternalTest, reachableFlag)
{
    Label label;
    label.setReachable(false);
    ASSERT_FALSE(label.isReachable());
    label.setReachable(true);
    ASSERT_TRUE(label.isReachable());
}

// ---- Label Stack Size Tests ----

TEST(LabelInternalTest, defaultStackSizes)
{
    Label label;
    ASSERT_EQ(-1, label.inputStackSize);
    ASSERT_EQ(-1, label.outputStackSize);
    ASSERT_EQ(-1, label.outputStackMax);
}

TEST(LabelInternalTest, setInputStackSize)
{
    Label label;
    label.inputStackSize = 5;
    ASSERT_EQ(5, label.inputStackSize);
    ASSERT_EQ(-1, label.outputStackSize);   // unchanged
}

TEST(LabelInternalTest, setOutputStackSize)
{
    Label label;
    label.outputStackSize = 3;
    ASSERT_EQ(3, label.outputStackSize);
}

TEST(LabelInternalTest, setOutputStackMax)
{
    Label label;
    label.outputStackMax = 10;
    ASSERT_EQ(10, label.outputStackMax);
}

// ---- Label Subroutine Tests ----

TEST(LabelInternalTest, defaultSubroutineId)
{
    Label label;
    ASSERT_EQ(0, label.subroutineId);
}

TEST(LabelInternalTest, setSubroutineId)
{
    Label label;
    label.subroutineId = 42;
    ASSERT_EQ(42, label.subroutineId);
}

// ---- Label Edge Tests ----

TEST(LabelInternalTest, defaultOutgoingEdges)
{
    Label label;
    ASSERT_EQ(nullptr, label.outgoingEdges);
}

TEST(LabelInternalTest, setOutgoingEdges)
{
    Label label;
    Label target;
    Edge  edge(Edge::JUMP, &target, nullptr);
    label.outgoingEdges = &edge;
    ASSERT_EQ(&edge, label.outgoingEdges);
    ASSERT_EQ(&target, label.outgoingEdges->successor);
}

TEST(LabelInternalTest, multipleOutgoingEdges)
{
    Label label;
    Label t1, t2;
    Edge  e2(Edge::EXCEPTION, &t2, nullptr);
    Edge  e1(Edge::JUMP, &t1, &e2);
    label.outgoingEdges = &e1;
    ASSERT_EQ(2, label.getOutgoingEdgeCount());
}

TEST(LabelInternalTest, emptyOutgoingEdgeCount)
{
    Label label;
    ASSERT_EQ(0, label.getOutgoingEdgeCount());
}

TEST(LabelInternalTest, outgoingEdgeCount)
{
    Label label;
    Label t1, t2, t3;
    Edge  e3(Edge::JUMP, &t3, nullptr);
    Edge  e2(Edge::JUMP, &t2, &e3);
    Edge  e1(Edge::JUMP, &t1, &e2);
    label.outgoingEdges = &e1;
    ASSERT_EQ(3, label.getOutgoingEdgeCount());
}

// ---- Label Next List Element Tests ----

TEST(LabelInternalTest, defaultNextListElement)
{
    Label label;
    ASSERT_EQ(nullptr, label.nextListElement);
}

TEST(LabelInternalTest, setNextListElement)
{
    Label l1, l2;
    l1.nextListElement = &l2;
    ASSERT_EQ(&l2, l1.nextListElement);
}

// ---- Label Combined Tests ----

TEST(LabelInternalTest, fullyFeaturedLabel)
{
    Label label;
    Label target;

    label.bytecodeOffset  = 100;
    label.line            = 42;
    label.inputStackSize  = 3;
    label.outputStackSize = 5;
    label.outputStackMax  = 8;
    label.subroutineId    = 7;
    label.setJumpTarget(true);
    label.setReachable(true);

    Edge edge(Edge::JUMP, &target, nullptr);
    label.outgoingEdges = &edge;

    ASSERT_EQ(100, label.bytecodeOffset);
    ASSERT_EQ(42, label.line);
    ASSERT_EQ(3, label.inputStackSize);
    ASSERT_EQ(5, label.outputStackSize);
    ASSERT_EQ(8, label.outputStackMax);
    ASSERT_EQ(7, label.subroutineId);
    ASSERT_TRUE(label.isJumpTarget());
    ASSERT_TRUE(label.isReachable());
    ASSERT_EQ(&edge, label.outgoingEdges);
}

// ---- ASM-style forward reference and resolve ----

TEST(LabelInternalTest, forwardRefResolution)
{
    Label label;
    ASSERT_FALSE(label.isResolved());

    // Simulate bytecode with a forward jump: at offset 10, a GOTO instruction
    // reserves 2 bytes for the jump offset. We register a forward ref.
    std::vector<ca::u8> code(20, 0);
    // Register forward reference: source offset = 10, short type, handle = 12 (where to patch)
    label.addForwardRef(10, Label::FORWARD_REFERENCE_TYPE_SHORT, 12);

    // Resolve the label at offset 42
    label.resolve(code.data(), 42);
    ASSERT_EQ(42, label.bytecodeOffset);
    ASSERT_TRUE(label.isResolved());

    // Verify the code was patched: offset = 42 - 10 = 32 = 0x0020
    EXPECT_EQ(code[12], 0x00);
    EXPECT_EQ(code[13], 0x20);
}

TEST(LabelInternalTest, infoPointerRegression)
{
    Label label;
    ASSERT_EQ(nullptr, label.info);
    int x      = 42;
    label.info = &x;
    ASSERT_EQ(&x, label.info);
}
