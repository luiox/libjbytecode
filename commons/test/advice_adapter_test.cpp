#include <gtest/gtest.h>
#include <vector>
#include "test_utils.hpp"

namespace {

// Minimal concrete AdviceAdapter subclass that records enter/exit calls.
struct TestAdviceAdapter : AdviceAdapter
{
    bool entered    = false;
    bool exited     = false;
    int  exitOpcode = -1;

    TestAdviceAdapter(jbytecode::core::MethodVisitor* mv, int access, ca::str::Utf8StringRef name,
                      ca::str::Utf8StringRef descriptor)
        : AdviceAdapter(Opcodes::ASM9, mv, access, name, descriptor)
    {}

    void onMethodEnter() override { entered = true; }
    void onMethodExit(int opcode) override
    {
        exited     = true;
        exitOpcode = opcode;
    }
};

struct RecordingVisitor : jbytecode::core::MethodVisitor
{
    RecordingVisitor()
        : jbytecode::core::MethodVisitor(Opcodes::ASM9, nullptr)
    {}
    std::vector<int> opcodes;
    void             visitInsn(int opcode) override { opcodes.push_back(opcode); }
    void             visitVarInsn(int opcode, int) override { opcodes.push_back(opcode); }
    void             visitMaxs(int, int) override {}
};

}   // namespace

TEST(AdviceAdapterTest, onMethodEnter_called_forNonConstructor)
{
    RecordingVisitor  rec;
    TestAdviceAdapter adv(&rec,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"));
    adv.visitCode();
    EXPECT_TRUE(adv.entered);
}

TEST(AdviceAdapterTest, onMethodExit_called_onReturn)
{
    RecordingVisitor  rec;
    TestAdviceAdapter adv(&rec,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"));
    adv.visitCode();
    adv.visitInsn(Opcodes::RETURN);
    EXPECT_TRUE(adv.exited);
    EXPECT_EQ(Opcodes::RETURN, adv.exitOpcode);
}

TEST(AdviceAdapterTest, onMethodExit_called_onIReturn)
{
    RecordingVisitor  rec;
    TestAdviceAdapter adv(&rec,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("m"),
                          ZUtf8StringRef::from_static("()V"));
    adv.visitCode();
    adv.visitInsn(Opcodes::IRETURN);
    EXPECT_TRUE(adv.exited);
    EXPECT_EQ(Opcodes::IRETURN, adv.exitOpcode);
}

TEST(AdviceAdapterTest, constructor_doesNotCallEnterOnVisitCode)
{
    // For constructors (<init>), onMethodEnter is deferred until super() call.
    RecordingVisitor  rec;
    TestAdviceAdapter adv(&rec,
                          Opcodes::ACC_PUBLIC,
                          ZUtf8StringRef::from_static("<init>"),
                          ZUtf8StringRef::from_static("()V"));
    adv.visitCode();
    EXPECT_FALSE(adv.entered);
}
