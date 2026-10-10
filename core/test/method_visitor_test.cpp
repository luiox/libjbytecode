#include <gtest/gtest.h>
#include "jbytecode/core/visitors.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;
using namespace ca::str;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）。
ca::str::Utf8StringArena arena;

struct LogMethodVisitor : MethodVisitor
{
    std::string* log;

    explicit LogMethodVisitor(std::string* log)
        : MethodVisitor(Opcodes::ASM10_EXPERIMENTAL)
        , log(log)
    {}

    using MethodVisitor::visitMethodInsn;

    void visitMethodInsn(int, ca::str::Utf8StringRef, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool) override
    {
        *log += "LogMethodVisitor:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

struct MethodVisitor4 : MethodVisitor
{
    explicit MethodVisitor4(MethodVisitor* mv)
        : MethodVisitor(Opcodes::ASM4, mv)
    {}
};

struct MethodVisitor4Override : MethodVisitor
{
    std::string* log;

    MethodVisitor4Override(MethodVisitor* mv, std::string* log)
        : MethodVisitor(Opcodes::ASM4, mv)
        , log(log)
    {}

    using MethodVisitor::visitMethodInsn;

    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor) override
    {
        std::string renamed(reinterpret_cast<const char*>(name.data()), name.byte_length());
        renamed += "4";
        MethodVisitor::visitMethodInsn(
            opcode, owner, ca::str::Utf8StringRef::from_cstr(renamed.c_str()), descriptor);
        *log += "MethodVisitor4:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

struct MethodVisitor5 : MethodVisitor
{
    explicit MethodVisitor5(MethodVisitor* mv)
        : MethodVisitor(Opcodes::ASM5, mv)
    {}
};

struct MethodVisitor5Override : MethodVisitor
{
    std::string* log;

    MethodVisitor5Override(MethodVisitor* mv, std::string* log)
        : MethodVisitor(Opcodes::ASM5, mv)
        , log(log)
    {}

    using MethodVisitor::visitMethodInsn;

    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override
    {
        std::string renamed(reinterpret_cast<const char*>(name.data()), name.byte_length());
        renamed += "5";
        MethodVisitor::visitMethodInsn(opcode,
                                       owner,
                                       ca::str::Utf8StringRef::from_cstr(renamed.c_str()),
                                       descriptor,
                                       isInterface);
        *log += "MethodVisitor5:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

struct TraceMethodVisitor : MethodVisitor
{
    std::string* log;

    TraceMethodVisitor(int api, MethodVisitor* mv, std::string* log)
        : MethodVisitor(api, mv)
        , log(log)
    {}

    using MethodVisitor::visitMethodInsn;

    void visitMethodInsn(int opcodeAndSource, ca::str::Utf8StringRef owner,
                         ca::str::Utf8StringRef name, ca::str::Utf8StringRef descriptor,
                         bool isInterface) override
    {
        if (api < Opcodes::ASM5 && (opcodeAndSource & Opcodes::SOURCE_DEPRECATED) == 0) {
            MethodVisitor::visitMethodInsn(opcodeAndSource, owner, name, descriptor, isInterface);
            return;
        }
        MethodVisitor::visitMethodInsn(opcodeAndSource, owner, name, descriptor, isInterface);
        *log += "TraceMethodVisitor:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

struct UserTraceMethodVisitor4 : TraceMethodVisitor
{
    UserTraceMethodVisitor4(MethodVisitor* mv, std::string* log)
        : TraceMethodVisitor(Opcodes::ASM4, mv, log)
    {}

    using TraceMethodVisitor::visitMethodInsn;

    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor) override
    {
        TraceMethodVisitor::visitMethodInsn(opcode, owner, name, descriptor);
        *log += "UserTraceMethodVisitor4:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

struct UserTraceMethodVisitor5 : TraceMethodVisitor
{
    UserTraceMethodVisitor5(MethodVisitor* mv, std::string* log)
        : TraceMethodVisitor(Opcodes::ASM5, mv, log)
    {}

    using TraceMethodVisitor::visitMethodInsn;

    void visitMethodInsn(int opcode, ca::str::Utf8StringRef owner, ca::str::Utf8StringRef name,
                         ca::str::Utf8StringRef descriptor, bool isInterface) override
    {
        TraceMethodVisitor::visitMethodInsn(opcode, owner, name, descriptor, isInterface);
        *log += "UserTraceMethodVisitor5:";
        *log += std::string(reinterpret_cast<const char*>(name.data()), name.byte_length());
        *log +=
            std::string(reinterpret_cast<const char*>(descriptor.data()), descriptor.byte_length());
        *log += ";";
    }
};

}   // namespace

TEST(MethodVisitorTest, testConstructor_validApi)
{
    ASSERT_NO_THROW({ MethodVisitor mv(Opcodes::ASM4); });
}

TEST(MethodVisitorTest, testConstructor_invalidApi)
{
    try {
        MethodVisitor mv(0);
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        ASSERT_STREQ("Unsupported api 0", e.what());
    }
}

TEST(MethodVisitorTest, testGetDelegate)
{
    MethodVisitor* delegate = new MethodVisitor(Opcodes::ASM4);
    MethodVisitor  visitor(Opcodes::ASM4, delegate);
    EXPECT_EQ(delegate, visitor.getDelegate());
    delete delegate;
}

TEST(MethodVisitorTest, testVisitParameter_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitParameter({}, 0);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitTypeAnnotation_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitTypeAnnotation(0, nullptr, {}, false);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitInvokeDynamicInsn_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitInvokeDynamicInsn({}, {}, Handle(0, {}, {}, {}), {});
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitInsnAnnotation_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitInsnAnnotation(0, nullptr, {}, false);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitTryCatchAnnotation_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitTryCatchAnnotation(0, nullptr, {}, false);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitLocalVariableAnnotation_asm4Visitor)
{
    MethodVisitor methodVisitor(Opcodes::ASM4, nullptr);

    try {
        methodVisitor.visitLocalVariableAnnotation(0, nullptr, {}, {}, {}, {}, false);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error& e) {
        EXPECT_STREQ("This feature requires ASM5", e.what());
    }
}

TEST(MethodVisitorTest, testVisitFrame_consecutiveFrames_sameFrame)
{
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("C"),
                      ZUtf8StringRef::from_static(""),
                      ZUtf8StringRef::from_static("D"),
                      {});
    MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                                           ZUtf8StringRef::from_static("m"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ZUtf8StringRef::from_static(""),
                                                           {});
    methodVisitor->visitCode();
    methodVisitor->visitFrame(Opcodes::F_SAME, 0, {}, 0, {});

    EXPECT_NO_THROW(methodVisitor->visitFrame(Opcodes::F_SAME, 0, {}, 0, {}));
}

TEST(MethodVisitorTest, testVisitFrame_consecutiveFrames)
{
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("C"),
                      ZUtf8StringRef::from_static(""),
                      ZUtf8StringRef::from_static("D"),
                      {});
    MethodVisitor* methodVisitor = classWriter.visitMethod(Opcodes::ACC_STATIC,
                                                           ZUtf8StringRef::from_static("m"),
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ZUtf8StringRef::from_static(""),
                                                           {});
    methodVisitor->visitCode();
    methodVisitor->visitFrame(Opcodes::F_SAME, 0, {}, 0, {});

    EXPECT_THROW(methodVisitor->visitFrame(Opcodes::F_APPEND, 1, {Opcodes::INTEGER}, 0, {}),
                 std::logic_error);
}

TEST(MethodVisitorTest, testVisitFrame_compressedFrameWithV1_5class)
{
    ClassWriter classWriter(arena, 0);
    classWriter.visit(Opcodes::V1_5,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("C"),
                      ZUtf8StringRef::from_static(""),
                      ZUtf8StringRef::from_static("D"),
                      {});
    ClassWriter    methodClassWriter(arena, 0);
    MethodVisitor* methodVisitor = methodClassWriter.visitMethod(Opcodes::ACC_STATIC,
                                                                 ZUtf8StringRef::from_static("m"),
                                                                 ZUtf8StringRef::from_static("()V"),
                                                                 ZUtf8StringRef::from_static(""),
                                                                 {});
    methodVisitor->visitCode();

    try {
        methodVisitor->visitFrame(Opcodes::F_SAME, 0, {}, 0, {});
        FAIL() << "Expected std::invalid_argument";
    }
    catch (const std::invalid_argument& e) {
        EXPECT_STREQ("Class versions V1_5 or less must use F_NEW frames.", e.what());
    }
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_asm4Visitor)
{
    std::string      log;
    LogMethodVisitor logMethodVisitor(&log);
    MethodVisitor4   methodVisitor(&logMethodVisitor);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_overridenAsm4Visitor)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor4Override methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m4()V;MethodVisitor4:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_asm5Visitor)
{
    std::string      log;
    LogMethodVisitor logMethodVisitor(&log);
    MethodVisitor5   methodVisitor(&logMethodVisitor);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_overridenAsm5Visitor)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor5Override methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m5()V;MethodVisitor5:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_userTraceMethodVisitor4)
{
    std::string             log;
    LogMethodVisitor        logMethodVisitor(&log);
    UserTraceMethodVisitor4 methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m()V;TraceMethodVisitor:m()V;UserTraceMethodVisitor4:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_userTraceMethodVisitor5)
{
    std::string             log;
    LogMethodVisitor        logMethodVisitor(&log);
    UserTraceMethodVisitor5 methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m()V;TraceMethodVisitor:m()V;UserTraceMethodVisitor5:m()V;", log);
}

TEST(MethodVisitorTest, testDeprecatedVisitMethodInsn_mixedVisitorChain)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor5         methodVisitor5(&logMethodVisitor);
    MethodVisitor5Override methodVisitor5Override(&methodVisitor5, &log);
    MethodVisitor4         methodVisitor4(&methodVisitor5Override);
    MethodVisitor4Override methodVisitor(&methodVisitor4, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"));

    EXPECT_EQ("LogMethodVisitor:m45()V;MethodVisitor5:m4()V;MethodVisitor4:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_asm4Visitor_isNotInterface)
{
    std::string      log;
    LogMethodVisitor logMethodVisitor(&log);
    MethodVisitor4   methodVisitor(&logMethodVisitor);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_asm4Visitor_isInterface)
{
    std::string      log;
    LogMethodVisitor logMethodVisitor(&log);
    MethodVisitor4   methodVisitor(&logMethodVisitor);

    EXPECT_THROW(methodVisitor.visitMethodInsn(0,
                                               ZUtf8StringRef::from_static("C"),
                                               ZUtf8StringRef::from_static("m"),
                                               ZUtf8StringRef::from_static("()V"),
                                               true),
                 std::runtime_error);
}

TEST(MethodVisitorTest, testVisitMethodInsn_overridenAsm4Visitor_isNotInterface)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor4Override methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m4()V;MethodVisitor4:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_overridenAsm4Visitor_isInterface)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor4Override methodVisitor(&logMethodVisitor, &log);

    EXPECT_THROW(methodVisitor.visitMethodInsn(0,
                                               ZUtf8StringRef::from_static("C"),
                                               ZUtf8StringRef::from_static("m"),
                                               ZUtf8StringRef::from_static("()V"),
                                               true),
                 std::runtime_error);
}

TEST(MethodVisitorTest, testVisitMethodInsn_asm5Visitor)
{
    std::string      log;
    LogMethodVisitor logMethodVisitor(&log);
    MethodVisitor5   methodVisitor(&logMethodVisitor);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_overridenAsm5Visitor)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor5Override methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m5()V;MethodVisitor5:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_userTraceMethodVisitor4)
{
    std::string             log;
    LogMethodVisitor        logMethodVisitor(&log);
    UserTraceMethodVisitor4 methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m()V;TraceMethodVisitor:m()V;UserTraceMethodVisitor4:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_userTraceMethodVisitor5)
{
    std::string             log;
    LogMethodVisitor        logMethodVisitor(&log);
    UserTraceMethodVisitor5 methodVisitor(&logMethodVisitor, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m()V;TraceMethodVisitor:m()V;UserTraceMethodVisitor5:m()V;", log);
}

TEST(MethodVisitorTest, testVisitMethodInsn_mixedVisitorChain)
{
    std::string            log;
    LogMethodVisitor       logMethodVisitor(&log);
    MethodVisitor4         methodVisitor4(&logMethodVisitor);
    MethodVisitor4Override methodVisitor4Override(&methodVisitor4, &log);
    MethodVisitor5         methodVisitor5(&methodVisitor4Override);
    MethodVisitor5Override methodVisitor(&methodVisitor5, &log);

    methodVisitor.visitMethodInsn(0,
                                  ZUtf8StringRef::from_static("C"),
                                  ZUtf8StringRef::from_static("m"),
                                  ZUtf8StringRef::from_static("()V"),
                                  false);

    EXPECT_EQ("LogMethodVisitor:m54()V;MethodVisitor4:m5()V;MethodVisitor5:m()V;", log);
}
