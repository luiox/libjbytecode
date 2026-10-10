#include <gtest/gtest.h>
#include "test_utils.hpp"

// TryCatchBlockSorter 在 visitEnd 将 try-catch 块按覆盖范围（标签距离）
// 升序排列：内层短块在前（Java 异常表语义要求精确匹配优先）。

TEST(TryCatchBlockSorterTest, sortsByBlockSpanAscending)
{
    // 构造方法：label0 label1 label2 label3（指令索引 0,1,2,3）
    TryCatchBlockSorter sorter(nullptr,
                               0,
                               ZUtf8StringRef::from_static("test"),
                               ZUtf8StringRef::from_static("()V"),
                               Utf8StringRef(),
                               {});
    // B8 收编语义：visitor 边界把外来堆 Label 拷贝为方法 arena 副本（owner 盖章，
    // 身份换新），原对象归调用方所有、调用方负责 delete。
    auto* l0 = new Label();
    auto* l1 = new Label();
    auto* l2 = new Label();
    auto* l3 = new Label();
    sorter.visitLabel(l0);
    sorter.visitInsn(Opcodes::NOP);
    sorter.visitLabel(l1);
    sorter.visitInsn(Opcodes::NOP);
    sorter.visitLabel(l2);
    sorter.visitInsn(Opcodes::NOP);
    sorter.visitLabel(l3);

    // 插入两个 try-catch：宽块(0→3 跨度 3) 和 窄块(1→2 跨度 1)
    auto* handler1 = new Label();
    auto* handler2 = new Label();
    sorter.visitTryCatchBlock(l0, l3, handler1, ZUtf8StringRef::from_static("java/lang/Exception"));
    sorter.visitTryCatchBlock(l1, l2, handler2, ZUtf8StringRef::from_static("java/lang/Throwable"));

    // visitEnd 排序：应按跨度升序 → 窄块在前
    sorter.visitEnd();

    ASSERT_EQ(2u, sorter.tryCatchBlocks.size());
    // 树内存的是收编副本：经 canonicalizeLabel 映射取回同一副本断言（身份幂等）。
    // 第一个应是窄块(l1→l2)，第二个是宽块(l0→l3)。
    EXPECT_EQ(sorter.canonicalizeLabel(l1), sorter.tryCatchBlocks[0]->start);
    EXPECT_EQ(sorter.canonicalizeLabel(l2), sorter.tryCatchBlocks[0]->end);
    EXPECT_EQ(sorter.canonicalizeLabel(l0), sorter.tryCatchBlocks[1]->start);
    EXPECT_EQ(sorter.canonicalizeLabel(l3), sorter.tryCatchBlocks[1]->end);
    delete l0;
    delete l1;
    delete l2;
    delete l3;
    delete handler1;
    delete handler2;
}

TEST(TryCatchBlockSorterTest, emptyBlocksNoOp)
{
    TryCatchBlockSorter sorter(nullptr,
                               0,
                               ZUtf8StringRef::from_static("test"),
                               ZUtf8StringRef::from_static("()V"),
                               Utf8StringRef(),
                               {});
    sorter.visitEnd();
    EXPECT_TRUE(sorter.tryCatchBlocks.empty());
}
