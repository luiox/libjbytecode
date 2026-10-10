#include <gtest/gtest.h>
#include "test_utils.hpp"

TEST(JSRInlinerAdapterTest, testConstructor)
{
    JSRInlinerAdapter jsr(nullptr,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"),
                          Utf8StringRef(),
                          {});
    EXPECT_EQ(ZUtf8StringRef::from_static("m"), jsr.name);
    EXPECT_EQ(ZUtf8StringRef::from_static("()V"), jsr.desc);
}

TEST(JSRInlinerAdapterTest, visitJumpInsn_delegatesToParent)
{
    JSRInlinerAdapter jsr(nullptr,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"),
                          Utf8StringRef(),
                          {});
    // B8 归属制：label 经 visitor 边界 canonicalizeLabel 拷贝收编为方法 arena 副本，
    // 原对象归调用方释放（树不再接管/delete 外来 Label）。
    auto* label = new jbytecode::core::Label();
    jsr.visitJumpInsn(Opcodes::GOTO, label);
    EXPECT_GT(jsr.instructions.size(), 0);
    delete label;   // 副本已收编进方法 arena，原件归调用方
}
