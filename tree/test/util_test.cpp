#include "garena/arena.hpp"
#include <gtest/gtest.h>
#include "test_utils.hpp"

// B8b：测试侧 Label 统一归宿（进程寿命 scratch arena）——LabelNode() 默认构造
// 已随旧池制度退役，Label 一律 arena 分配。
static garena::Arena& labelScratchArena()
{
    static garena::Arena arena;
    return arena;
}



TEST(UtilTest, testAsArrayList_nullArray)
{
    InsnList list;
    EXPECT_TRUE(list.toArray().empty());
}

TEST(UtilTest, testAsArrayList_withLength)
{
    InsnList other;
    other.add(new InsnNode(Opcodes::NOP));
    other.add(new InsnNode(Opcodes::RETURN));

    InsnList list;
    list.add(other);

    EXPECT_EQ(list.size(), 2);
    EXPECT_TRUE(other.isEmpty());
    ASSERT_NE(list.getFirst(), nullptr);
    ASSERT_NE(list.getLast(), nullptr);
    EXPECT_EQ(list.getFirst()->getOpcode(), Opcodes::NOP);
    EXPECT_EQ(list.getLast()->getOpcode(), Opcodes::RETURN);
}

TEST(UtilTest, testAsArrayList_withLengthAndArray)
{
    auto* labelNode                  = new LabelNode(labelScratchArena().create<Label>());
    labelNode->label->bytecodeOffset = 123;

    InsnList list;
    list.add(new InsnNode(Opcodes::NOP));
    list.add(labelNode);
    list.add(new InsnNode(Opcodes::RETURN));

    list.resetLabels();

    EXPECT_EQ(labelNode->label->bytecodeOffset, -1);
}
