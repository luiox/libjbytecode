#include <gtest/gtest.h>
#include "test_utils.hpp"

// LocalVariablesSorter 按方法签名+访问标志算出首个可分配的局部槽，
// newLocal 顺序分配并按类型尺寸推进（long/double 占 2 槽）。
// mv 传 nullptr：只测分配逻辑。

TEST(LocalVariablesSorterTest, staticMethodFirstFreeSlot)
{
    // static (II)V：参数占槽 0、1 → 下一个可分配槽 = 2
    LocalVariablesSorter s(Opcodes::ACC_STATIC, ZUtf8StringRef::from_static("(II)V"), nullptr);
    EXPECT_EQ(2, s.newLocal(Type::INT_TYPE));
}

TEST(LocalVariablesSorterTest, instanceMethodReservesThis)
{
    // 实例 (II)V：this 占槽 0，参数占 1、2 → 下一个 = 3
    LocalVariablesSorter s(0, ZUtf8StringRef::from_static("(II)V"), nullptr);
    EXPECT_EQ(3, s.newLocal(Type::INT_TYPE));
}

TEST(LocalVariablesSorterTest, newLocalAdvancesBySize)
{
    LocalVariablesSorter s(Opcodes::ACC_STATIC, ZUtf8StringRef::from_static("()V"), nullptr);
    int                  a = s.newLocal(Type::INT_TYPE);    // 占 1 槽
    int                  b = s.newLocal(Type::LONG_TYPE);   // 占 2 槽
    int                  c = s.newLocal(Type::INT_TYPE);
    EXPECT_EQ(0, a);
    EXPECT_EQ(1, b);
    EXPECT_EQ(3, c);   // long 之后跳过一槽
}

TEST(LocalVariablesSorterTest, longArgsCountAsTwoSlots)
{
    // static (JI)V：long 占槽 0-1，int 占槽 2 → 下一个 = 3
    LocalVariablesSorter s(Opcodes::ACC_STATIC, ZUtf8StringRef::from_static("(JI)V"), nullptr);
    EXPECT_EQ(3, s.newLocal(Type::INT_TYPE));
}
