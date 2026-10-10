#include <gtest/gtest.h>
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;
using namespace ca::str;

namespace {

// 全测试套共享 arena（gtest 串行，intern 累积无害）。
ca::str::Utf8StringArena arena;

struct Maxs
{
    int stack  = -1;
    int locals = -1;
};

struct FrameCountMethodVisitor : MethodVisitor
{
    int* count;

    explicit FrameCountMethodVisitor(int* count)
        : MethodVisitor(Opcodes::ASM5)
        , count(count)
    {}

    void visitFrame(int, int, const std::vector<ca::core::Any>&, int,
                    const std::vector<ca::core::Any>&) override
    {
        ++*count;
    }
};

struct FrameCountClassVisitor : ClassVisitor
{
    FrameCountMethodVisitor methodVisitor;

    explicit FrameCountClassVisitor(int* count)
        : ClassVisitor(Opcodes::ASM5)
        , methodVisitor(count)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return name.equals(ZUtf8StringRef::from_static("loop")) ? &methodVisitor : nullptr;
    }
};

struct MaxsMethodVisitor : MethodVisitor
{
    Maxs* maxs;

    explicit MaxsMethodVisitor(Maxs* maxs)
        : MethodVisitor(Opcodes::ASM5)
        , maxs(maxs)
    {}

    void visitMaxs(int stack, int locals) override
    {
        maxs->stack  = stack;
        maxs->locals = locals;
    }
};

struct MaxsClassVisitor : ClassVisitor
{
    ca::str::Utf8StringRef targetMethod;
    Maxs*                  maxs;
    MaxsMethodVisitor      methodVisitor;

    MaxsClassVisitor(ca::str::Utf8StringRef targetMethod, Maxs* maxs)
        : ClassVisitor(Opcodes::ASM5)
        , targetMethod(targetMethod)
        , maxs(maxs)
        , methodVisitor(maxs)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        if (name.byte_length() == targetMethod.byte_length() &&
            std::memcmp(name.data(), targetMethod.data(), name.byte_length()) == 0) {
            return &methodVisitor;
        }
        return nullptr;
    }
};

struct DummyClass
{
    ClassWriter* writer;

    explicit DummyClass(ClassWriter* writer)
        : writer(writer)
    {
        writer->visit(Opcodes::V1_7,
                      Opcodes::ACC_PUBLIC,
                      ZUtf8StringRef::from_static("SomeClass"),
                      ZUtf8StringRef::from_static(""),
                      ZUtf8StringRef::from_static("java/lang/Object"),
                      {});
    }

    DummyClass& withDummyMethod(ca::str::Utf8StringRef method)
    {
        Label          start;
        MethodVisitor* methodVisitor = writer->visitMethod(Opcodes::ACC_PUBLIC,
                                                           method,
                                                           ZUtf8StringRef::from_static("()V"),
                                                           ZUtf8StringRef::from_static(""),
                                                           {});
        methodVisitor->visitCode();
        methodVisitor->visitLabel(&start);
        methodVisitor->visitInsn(Opcodes::LCONST_0);
        Label label;
        methodVisitor->visitJumpInsn(Opcodes::GOTO, &label);
        methodVisitor->visitLabel(&label);
        methodVisitor->visitFrame(Opcodes::F_NEW, 0, {}, 1, {Opcodes::LONG});
        methodVisitor->visitInsn(Opcodes::ACONST_NULL);
        methodVisitor->visitInsn(Opcodes::RETURN);
        methodVisitor->visitMaxs(1, 0);
        methodVisitor->visitEnd();
        return *this;
    }

    std::vector<uint8_t> compile()
    {
        writer->visitEnd();
        return writer->toByteArray();
    }
};

Maxs readMaxs(const std::vector<uint8_t>& bytes, ca::str::Utf8StringRef method)
{
    Maxs                     maxs;
    ca::str::Utf8StringArena arena;
    ClassReader              reader(arena, bytes);
    MaxsClassVisitor         visitor(method, &maxs);
    reader.accept(&visitor, 0);
    return maxs;
}

Maxs compileAndReadMaxs(ClassWriter* writer, ca::str::Utf8StringRef method)
{
    auto bytes = DummyClass(writer).withDummyMethod(method).compile();
    return readMaxs(bytes, method);
}

}   // namespace

TEST(ClassWriterFlagsTest, switchesModesThreeTimes)
{
    ClassWriter writer(arena, 0);

    writer.setFlags(ClassWriter::COMPUTE_MAXS);
    ASSERT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
    ASSERT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));

    writer.setFlags(0);
    ASSERT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
    ASSERT_FALSE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));

    writer.setFlags(ClassWriter::COMPUTE_FRAMES);
    ASSERT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_MAXS));
    ASSERT_TRUE(writer.hasFlags(ClassWriter::COMPUTE_FRAMES));
}

TEST(ClassWriterFlagsTest, computesFramesAsUsual)
{
    ClassWriter writer(arena, ClassWriter::COMPUTE_FRAMES);
    Maxs        maxs = compileAndReadMaxs(&writer, ZUtf8StringRef::from_static("frames"));

    EXPECT_EQ(3, maxs.stack);
    EXPECT_EQ(1, maxs.locals);
}

TEST(ClassWriterFlagsTest, emitsFrameForBackwardTargetAliasedAtSameOffset)
{
    ClassWriter writer(arena, ClassWriter::COMPUTE_FRAMES);
    writer.visit(Opcodes::V1_7,
                 Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("BackwardTarget"),
                 {},
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
    MethodVisitor* method = writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                               ZUtf8StringRef::from_static("loop"),
                                               ZUtf8StringRef::from_static("(I)V"),
                                               {},
                                               {});
    Label          loop;
    Label          end;
    method->visitCode();
    method->visitVarInsn(Opcodes::ILOAD, 0);
    method->visitJumpInsn(Opcodes::IFEQ, &end);
    method->visitLabel(&loop);
    method->visitIincInsn(0, -1);
    method->visitVarInsn(Opcodes::ILOAD, 0);
    method->visitJumpInsn(Opcodes::IFNE, &loop);
    method->visitLabel(&end);
    method->visitInsn(Opcodes::RETURN);
    method->visitMaxs(0, 0);
    method->visitEnd();
    writer.visitEnd();

    const auto             bytes      = writer.toByteArray();
    int                    frameCount = 0;
    ClassReader            reader(arena, bytes);
    FrameCountClassVisitor visitor(&frameCount);
    reader.accept(&visitor, 0);
    EXPECT_EQ(2, frameCount);
}

TEST(ClassWriterFlagsTest, includesExceptionHandlerEntryInComputedMaxStack)
{
    ClassWriter writer(arena, ClassWriter::COMPUTE_FRAMES);
    writer.visit(Opcodes::V1_7,
                 Opcodes::ACC_PUBLIC,
                 ZUtf8StringRef::from_static("HandlerStack"),
                 {},
                 ZUtf8StringRef::from_static("java/lang/Object"),
                 {});
    MethodVisitor* method = writer.visitMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                                               ZUtf8StringRef::from_static("guarded"),
                                               ZUtf8StringRef::from_static("()V"),
                                               {},
                                               {});
    Label          start;
    Label          end;
    Label          handler;
    method->visitCode();
    method->visitTryCatchBlock(
        &start, &end, &handler, ZUtf8StringRef::from_static("java/lang/Exception"));
    method->visitLabel(&start);
    method->visitInsn(Opcodes::NOP);
    method->visitLabel(&end);
    method->visitInsn(Opcodes::RETURN);
    method->visitLabel(&handler);
    method->visitInsn(Opcodes::POP);
    method->visitInsn(Opcodes::RETURN);
    method->visitMaxs(0, 0);
    method->visitEnd();
    writer.visitEnd();

    const auto bytes = writer.toByteArray();
    const Maxs maxs  = readMaxs(bytes, ZUtf8StringRef::from_static("guarded"));
    EXPECT_EQ(1, maxs.stack);
    EXPECT_EQ(0, maxs.locals);
}

TEST(ClassWriterFlagsTest, computesMaxsAsUsual)
{
    ClassWriter writer(arena, ClassWriter::COMPUTE_MAXS);
    Maxs        maxs = compileAndReadMaxs(&writer, ZUtf8StringRef::from_static("maxs"));

    EXPECT_EQ(3, maxs.stack);
    EXPECT_EQ(1, maxs.locals);
}

TEST(ClassWriterFlagsTest, computesNothingAsUsual)
{
    ClassWriter writer(arena, 0);
    Maxs        maxs = compileAndReadMaxs(&writer, ZUtf8StringRef::from_static("nothing"));

    EXPECT_EQ(1, maxs.stack);
    EXPECT_EQ(0, maxs.locals);
}
