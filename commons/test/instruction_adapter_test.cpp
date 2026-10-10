#include <gtest/gtest.h>
#include <vector>
#include "test_utils.hpp"

namespace {

// 记录型 MethodVisitor：捕获被委托的指令，用于验证 InstructionAdapter
// 的便捷方法映射到正确的底层 visitXxx。
struct RecordingVisitor : jbytecode::core::MethodVisitor
{
    RecordingVisitor()
        : jbytecode::core::MethodVisitor(Opcodes::ASM9, nullptr)
    {}

    struct Insn
    {
        int kind;
        int opcode;
        int operand;
    };
    // kind: 0=visitInsn 1=visitIntInsn 2=visitLdcInsn
    std::vector<Insn> recorded;

    void visitInsn(int opcode) override { recorded.push_back({0, opcode, 0}); }
    void visitIntInsn(int opcode, int operand) override
    {
        recorded.push_back({1, opcode, operand});
    }
    void visitLdcInsn(const ca::core::Any&) override { recorded.push_back({2, 0, 0}); }
};

}   // namespace

TEST(InstructionAdapterTest, iconstSmallUsesIConstN)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.iconst(0);
    ia.iconst(5);
    ia.iconst(-1);
    ASSERT_EQ(3u, rec.recorded.size());
    EXPECT_EQ(0, rec.recorded[0].kind);
    EXPECT_EQ(Opcodes::ICONST_0, rec.recorded[0].opcode);
    EXPECT_EQ(Opcodes::ICONST_0 + 5, rec.recorded[1].opcode);
    EXPECT_EQ(Opcodes::ICONST_0 + (-1), rec.recorded[2].opcode);
}

TEST(InstructionAdapterTest, iconstByteRangeUsesBipush)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.iconst(100);   // [-128,127] 且超出 [-1,5] → BIPUSH
    ASSERT_EQ(1u, rec.recorded.size());
    EXPECT_EQ(1, rec.recorded[0].kind);   // visitIntInsn
    EXPECT_EQ(Opcodes::BIPUSH, rec.recorded[0].opcode);
    EXPECT_EQ(100, rec.recorded[0].operand);
}

TEST(InstructionAdapterTest, iconstShortRangeUsesSipush)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.iconst(1000);   // [-32768,32767] → SIPUSH
    ASSERT_EQ(1u, rec.recorded.size());
    EXPECT_EQ(1, rec.recorded[0].kind);
    EXPECT_EQ(Opcodes::SIPUSH, rec.recorded[0].opcode);
    EXPECT_EQ(1000, rec.recorded[0].operand);
}

TEST(InstructionAdapterTest, iconstLargeUsesLdc)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.iconst(100000);   // 超出 short → LDC
    ASSERT_EQ(1u, rec.recorded.size());
    EXPECT_EQ(2, rec.recorded[0].kind);   // visitLdcInsn
}

TEST(InstructionAdapterTest, stackOpsDelegateToVisitInsn)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.pop();
    ia.dup();
    ia.swap();
    ASSERT_EQ(3u, rec.recorded.size());
    EXPECT_EQ(Opcodes::POP, rec.recorded[0].opcode);
    EXPECT_EQ(Opcodes::DUP, rec.recorded[1].opcode);
    EXPECT_EQ(Opcodes::SWAP, rec.recorded[2].opcode);
}

TEST(InstructionAdapterTest, nopDelegates)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    ia.nop();
    ASSERT_EQ(1u, rec.recorded.size());
    EXPECT_EQ(Opcodes::NOP, rec.recorded[0].opcode);
}

// 回归 #742：真实编译管线里 STRING 常量以 Utf8StringRef 标签落入 ldc 值
// （见 libjblw conversion.cpp unwrapConstant），而非 std::string。
// InstructionAdapter::visitLdcInsn 曾用 value.is<std::string>() 路由字符串分支，
// 永远为 false，导致 Utf8StringRef ldc 落入末尾 throw "Illegal LDC value type"。
// 此处用真实管线产出的 Utf8StringRef 标签验证它能正确路由到 aconst。
TEST(InstructionAdapterTest, visitLdcInsnRoutesUtf8StringRefToAconst)
{
    RecordingVisitor   rec;
    InstructionAdapter ia(&rec);
    // 与真实管线一致：Utf8StringRef 标签（非 ZUtf8StringRef / std::string）
    EXPECT_NO_THROW(ia.visitLdcInsn(Utf8StringRef::from_cstr("hi")));
    ASSERT_EQ(1u, rec.recorded.size());
    EXPECT_EQ(2, rec.recorded[0].kind);   // visitLdcInsn
}
