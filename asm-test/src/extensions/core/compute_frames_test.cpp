#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

#include "test_helper.hpp"
#include "test_utils.hpp"
#include "jbytecode/core/writers.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/opcodes.hpp"
#include "jbytecode/core/constants.hpp"
#include "jbytecode/core/frame.hpp"
#include "jbytecode/core/type.hpp"
#include "jbytecode/core/exceptions.hpp"
#include "jbytecode/core/bounded_common_super_class_provider.hpp"
#include "jbytecode/tree/nodes.hpp"

using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

ca::str::Utf8StringRef ref(const char* s)
{
    auto len = std::strlen(s);
    return ca::str::Utf8StringRef(reinterpret_cast<const ca::u8*>(s), len, len);
}

std::string refToString(ca::str::Utf8StringRef value)
{
    return std::string(reinterpret_cast<const char*>(value.data()), value.byte_length());
}

uint32_t readMagic(const std::vector<uint8_t>& data)
{
    if (data.size() < 4) return 0;
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

void assertReadableClass(const std::vector<uint8_t>& bytes)
{
    ASSERT_GT(bytes.size(), static_cast<size_t>(20));
    ASSERT_EQ(0xCAFEBABE, readMagic(bytes));
    ClassNode   cn;
    ClassReader reader(asmArena, bytes.data(), 0, static_cast<ca::i32>(bytes.size()));
    ASSERT_NO_THROW(reader.accept(&cn, 0));
}

struct FirstFrameLocalVisitor : MethodVisitor
{
    std::string firstLocal;

    FirstFrameLocalVisitor()
        : MethodVisitor(Opcodes::ASM9)
    {}

    void visitFrame(int, int numLocal, const std::vector<ca::core::Any>& local, int,
                    const std::vector<ca::core::Any>&) override
    {
        if (!firstLocal.empty() || numLocal <= 0 || local.empty()) return;
        // Object verification types in frames are carried as Utf8StringRef
        // (the internal class name) after the libca string migration.
        if (auto* p = local[0].as<ca::str::Utf8StringRef>()) {
            firstLocal = refToString(*p);
        }
        else if (auto* p = local[0].as<std::string>()) {
            firstLocal = *p;
        }
    }
};

struct MethodFrameLookupVisitor : ClassVisitor
{
    std::string            methodName;
    FirstFrameLocalVisitor frameVisitor;

    explicit MethodFrameLookupVisitor(ca::str::Utf8StringRef name)
        : ClassVisitor(Opcodes::ASM9)
        , methodName(refToString(name))
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return refToString(name) == methodName ? &frameVisitor : nullptr;
    }
};

struct StackFrameVisitor : MethodVisitor
{
    std::vector<std::vector<ca::core::Any>> frames;

    StackFrameVisitor()
        : MethodVisitor(Opcodes::ASM9)
    {}

    void visitFrame(int, int, const std::vector<ca::core::Any>&, int numStack,
                    const std::vector<ca::core::Any>& stack) override
    {
        const auto count = (std::min)(static_cast<std::size_t>(numStack), stack.size());
        frames.emplace_back(stack.begin(), stack.begin() + count);
    }
};

struct StackFrameLookupVisitor : ClassVisitor
{
    StackFrameVisitor frameVisitor;

    StackFrameLookupVisitor()
        : ClassVisitor(Opcodes::ASM9)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return refToString(name) == "test" ? &frameVisitor : nullptr;
    }
};

}   // namespace

// Helper: create a ClassWriter, add a method with given instructions, write to byte array
struct CwBuilder
{
    ClassWriter   cw;
    MethodWriter* mv = nullptr;

    CwBuilder(int classVersion, ca::str::Utf8StringRef className,
              ca::str::Utf8StringRef superName = ref("java/lang/Object"))
        : cw(asmArena, ClassWriter::COMPUTE_FRAMES)
    {
        cw.visit(classVersion, Opcodes::ACC_PUBLIC, className, ref(""), superName, {});
    }

    void beginMethod(int access, ca::str::Utf8StringRef name, ca::str::Utf8StringRef desc)
    {
        mv = static_cast<MethodWriter*>(cw.visitMethod(access, name, desc, ref(""), {}));
        mv->visitCode();
    }

    void endMethod()
    {
        mv->visitMaxs(0, 0);
        mv->visitEnd();
    }

    std::vector<uint8_t> toBytes()
    {
        cw.visitEnd();
        auto buf = cw.toByteArray();
        return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
    }
};

// ======================================================================
// COMPUTE_FRAMES tests — basic method patterns
// ======================================================================

TEST(ComputeFramesTest, simpleReturn)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()V"));
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, intConstArithmetic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()I"));
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::ICONST_2);
    cb.mv->visitInsn(Opcodes::IADD);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, longArithmetic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()J"));
    cb.mv->visitInsn(Opcodes::LCONST_0);
    cb.mv->visitInsn(Opcodes::LCONST_1);
    cb.mv->visitInsn(Opcodes::LADD);
    cb.mv->visitInsn(Opcodes::LRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, floatArithmetic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()F"));
    cb.mv->visitInsn(Opcodes::FCONST_0);
    cb.mv->visitInsn(Opcodes::FCONST_1);
    cb.mv->visitInsn(Opcodes::FADD);
    cb.mv->visitInsn(Opcodes::FRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, doubleArithmetic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()D"));
    cb.mv->visitInsn(Opcodes::DCONST_0);
    cb.mv->visitInsn(Opcodes::DCONST_1);
    cb.mv->visitInsn(Opcodes::DADD);
    cb.mv->visitInsn(Opcodes::DRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, instanceMethod)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC, ref("test"), ref("()I"));
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, instanceMethodFrameUsesCurrentClassName)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC, ref("test"), ref("()I"));
    Label* target = new Label();
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitVarInsn(Opcodes::ISTORE, 2);
    cb.mv->visitJumpInsn(Opcodes::GOTO, target);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(target);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 2);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();

    MethodFrameLookupVisitor cv(ref("test"));
    ClassReader              reader(asmArena, bytes.data(), 0, static_cast<ca::i32>(bytes.size()));
    reader.accept(&cv, ClassReader::EXPAND_FRAMES);

    ASSERT_EQ("A", cv.frameVisitor.firstLocal);
}

TEST(ComputeFramesTest, staticMethodWithParams)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("add"), ref("(II)I"));
    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 1);
    cb.mv->visitInsn(Opcodes::IADD);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, methodWithLongParam)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(J)J"));
    cb.mv->visitVarInsn(Opcodes::LLOAD, 0);
    cb.mv->visitInsn(Opcodes::LCONST_1);
    cb.mv->visitInsn(Opcodes::LADD);
    cb.mv->visitInsn(Opcodes::LRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, branchIf)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(I)I"));
    Label* l1 = new Label();
    Label* l2 = new Label();

    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);

    // Need to resolve labels with put
    cb.mv->visitJumpInsn(Opcodes::IFGE, l1);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(l1);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, conditionalBranchPreservesLiveReferenceStack)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_8, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                   ref("test"),
                   ref("([Ljava/lang/String;)V"));
    Label* nonEmpty = new Label();
    Label* print    = new Label();
    cb.mv->visitFieldInsn(Opcodes::GETSTATIC,
                          ref("java/lang/System"),
                          ref("out"),
                          ref("Ljava/io/PrintStream;"));
    cb.mv->visitVarInsn(Opcodes::ALOAD, 0);
    cb.mv->visitInsn(Opcodes::ARRAYLENGTH);
    cb.mv->visitJumpInsn(Opcodes::IFNE, nonEmpty);
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitJumpInsn(Opcodes::GOTO, print);
    cb.mv->visitLabel(nonEmpty);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitLabel(print);
    cb.mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                           ref("java/io/PrintStream"),
                           ref("println"),
                           ref("(I)V"),
                           false);
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.endMethod();

    const auto bytes = cb.toBytes();
    StackFrameLookupVisitor visitor;
    ClassReader reader(asmArena, bytes.data(), 0, static_cast<ca::i32>(bytes.size()));
    reader.accept(&visitor, ClassReader::EXPAND_FRAMES);

    ASSERT_EQ(visitor.frameVisitor.frames.size(), 2U);
    ASSERT_EQ(visitor.frameVisitor.frames[0].size(), 1U);
    const auto* first = visitor.frameVisitor.frames[0][0].as<ca::str::Utf8StringRef>();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(refToString(*first), "java/io/PrintStream");
    ASSERT_EQ(visitor.frameVisitor.frames[1].size(), 2U);
    const auto* second = visitor.frameVisitor.frames[1][0].as<ca::str::Utf8StringRef>();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(refToString(*second), "java/io/PrintStream");
    const auto* flag = visitor.frameVisitor.frames[1][1].as<int>();
    ASSERT_NE(flag, nullptr);
    EXPECT_EQ(*flag, Opcodes::INTEGER);
}

TEST(ComputeFramesTest, gotoJump)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()V"));
    Label* l = new Label();
    cb.mv->visitJumpInsn(Opcodes::GOTO, l);
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.mv->visitLabel(l);
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, ldcString)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()Ljava/lang/String;"));
    cb.mv->visitLdcInsn(std::string("hello"));
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, ldcInt)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()I"));
    cb.mv->visitLdcInsn(42);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, newObject)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()Ljava/lang/Object;"));
    cb.mv->visitTypeInsn(Opcodes::NEW, ref("java/lang/Object"));
    cb.mv->visitInsn(Opcodes::DUP);
    cb.mv->visitMethodInsn(
        Opcodes::INVOKESPECIAL, ref("java/lang/Object"), ref("<init>"), ref("()V"), false);
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, invokeStatic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()I"));
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::ICONST_2);
    cb.mv->visitMethodInsn(
        Opcodes::INVOKESTATIC, ref("java/lang/Math"), ref("max"), ref("(II)I"), false);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, newArray)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()[I"));
    cb.mv->visitInsn(Opcodes::ICONST_5);
    cb.mv->visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_INT);
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, anewArray)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()[Ljava/lang/String;"));
    cb.mv->visitInsn(Opcodes::ICONST_3);
    cb.mv->visitTypeInsn(Opcodes::ANEWARRAY, ref("java/lang/String"));
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, iincInsn)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(I)I"));
    cb.mv->visitIincInsn(0, 1);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, tryCatch)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()I"));
    Label* start   = new Label();
    Label* end     = new Label();
    Label* handler = new Label();

    cb.mv->visitLabel(start);
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(end);
    cb.mv->visitLabel(handler);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitTryCatchBlock(start, end, handler, ref("java/lang/Exception"));
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, ifNullBranch)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(Ljava/lang/Object;)Z"));
    Label* l = new Label();
    cb.mv->visitVarInsn(Opcodes::ALOAD, 0);
    cb.mv->visitJumpInsn(Opcodes::IFNULL, l);
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(l);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, tableSwitch)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(I)I"));
    Label* l1   = new Label();
    Label* l2   = new Label();
    Label* dflt = new Label();

    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitTableSwitchInsn(0, 1, dflt, {l1, l2});
    cb.mv->visitLabel(l1);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(l2);
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(dflt);
    cb.mv->visitInsn(Opcodes::ICONST_M1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, nestedBranches)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(I)I"));
    Label* pos = new Label();
    Label* neg = new Label();

    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitJumpInsn(Opcodes::IFGT, pos);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitJumpInsn(Opcodes::IFLT, neg);
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(pos);
    cb.mv->visitInsn(Opcodes::ICONST_1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.mv->visitLabel(neg);
    cb.mv->visitInsn(Opcodes::ICONST_M1);
    cb.mv->visitInsn(Opcodes::IRETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, checkCast)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()Ljava/lang/Object;"));
    cb.mv->visitTypeInsn(Opcodes::NEW, ref("java/lang/Object"));
    cb.mv->visitInsn(Opcodes::DUP);
    cb.mv->visitMethodInsn(
        Opcodes::INVOKESPECIAL, ref("java/lang/Object"), ref("<init>"), ref("()V"), false);
    cb.mv->visitTypeInsn(Opcodes::CHECKCAST, ref("java/lang/Object"));
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, getStaticPutStatic)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    // Add a static field
    cb.cw
        .visitField(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                    ref("value"),
                    ref("I"),
                    ref(""),
                    ca::core::Any())
        ->visitEnd();

    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("(I)V"));
    cb.mv->visitVarInsn(Opcodes::ILOAD, 0);
    cb.mv->visitFieldInsn(Opcodes::PUTSTATIC, ref("A"), ref("value"), ref("I"));
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, getFieldPutField)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    // Add an instance field
    cb.cw.visitField(Opcodes::ACC_PUBLIC, ref("value"), ref("I"), ref(""), ca::core::Any())
        ->visitEnd();

    cb.beginMethod(Opcodes::ACC_PUBLIC, ref("test"), ref("(I)V"));
    cb.mv->visitVarInsn(Opcodes::ALOAD, 0);
    cb.mv->visitVarInsn(Opcodes::ILOAD, 1);
    cb.mv->visitFieldInsn(Opcodes::PUTFIELD, ref("A"), ref("value"), ref("I"));
    cb.mv->visitInsn(Opcodes::RETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, invokeVirtual)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC, ref("test"), ref("()Ljava/lang/String;"));
    cb.mv->visitVarInsn(Opcodes::ALOAD, 0);
    cb.mv->visitMethodInsn(Opcodes::INVOKEVIRTUAL,
                           ref("java/lang/Object"),
                           ref("toString"),
                           ref("()Ljava/lang/String;"),
                           false);
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

TEST(ComputeFramesTest, multiANewArray)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()[[I"));
    cb.mv->visitInsn(Opcodes::ICONST_3);
    cb.mv->visitInsn(Opcodes::ICONST_4);
    cb.mv->visitMultiANewArrayInsn(ref("[[I"), 2);
    cb.mv->visitInsn(Opcodes::ARETURN);
    cb.endMethod();
    auto bytes = cb.toBytes();
    assertReadableClass(bytes);
}

// Verify typeResolver is forwarded from ClassWriter → MethodWriter → Frame::merge
struct CallbackProvider : ICommonSuperClassProvider
{
    int                    callCount = 0;
    ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef& t1,
                                               const ca::str::Utf8StringRef& t2) override
    {
        ++callCount;
        (void)t1;
        (void)t2;
        static const ca::u8                 kObj[] = "java/lang/Object";
        static const ca::str::Utf8StringRef kObjRef(kObj, 16, 16);
        return kObjRef;
    }
};

// Direct test: verify that Frame::merge (public) forwards typeResolver to addMergedType.
TEST(ComputeFramesTest, typeResolverIsForwarded)
{
    CallbackProvider resolver;
    SymbolTable      symbolTable(asmArena);

    // Register types in the symbol table
    int stringIdx = symbolTable.addType(ref("java/lang/String"));
    int objectIdx = symbolTable.addType(ref("java/lang/Object"));
    ASSERT_NE(stringIdx, objectIdx);

    // Build abstract types: REFERENCE_KIND | typeIndex
    int stringAType = AbstractType::REFERENCE_KIND | stringIdx;
    int objectAType = AbstractType::REFERENCE_KIND | objectIdx;

    // Create source frame: outputLocals[0] = String type
    Frame srcFrame(nullptr);
    srcFrame.outputLocals.push_back(stringAType);
    srcFrame.inputLocals.push_back(stringAType);

    // Create dst frame: inputLocals[0] = Object type (different from source)
    Frame dstFrame(nullptr);
    dstFrame.inputLocals.push_back(objectAType);

    // Merge source's output into dst's input
    // This should trigger addMergedType(String, Object) → calls getCommonSuperClass
    bool changed = srcFrame.merge(symbolTable, dstFrame, 0, &resolver);

    // The merge should have triggered getCommonSuperClass because String ≠ Object
    EXPECT_GT(resolver.callCount, 0) << "typeResolver not forwarded through Frame::merge";
}

// #575 兜底：装饰器预算——包住一个"非单调"provider（每次返回递增类型名，
// 模拟继承环/类型组合爆炸等不收敛缺陷），预算耗尽必须抛
// CommonSuperClassBudgetExceeded，而不是无限调用。
struct DivergingProvider : ICommonSuperClassProvider
{
    int         callCount = 0;
    std::string last;   // 成员存储：返回的 Utf8StringRef 必须存活到下次调用
    ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef&,
                                               const ca::str::Utf8StringRef&) override
    {
        ++callCount;
        last = "Loop" + std::to_string(callCount);
        return ca::str::Utf8StringRef::from_cstr(last.c_str());
    }
};

TEST(ComputeFramesTest, boundedProviderThrowsOnBudgetExceeded)
{
    DivergingProvider inner;
    BoundedCommonSuperClassProvider bounded(&inner, 16);
    EXPECT_THROW(
        {
            for (int i = 0; i < 100; ++i) {
                bounded.getCommonSuperClass(ref("A"), ref("B"));
            }
        },
        CommonSuperClassBudgetExceeded);
    EXPECT_EQ(inner.callCount, 16);   // 前 16 次进入 inner，第 17 次在装饰器层抛
}

// #575 兜底集成：COMPUTE_FRAMES 下 provider 抛预算超限（或任何 provider 内
// 死循环被预算截断）必须转 FrameComputationException（供调用方 SKIP 降级），
// 而不是进程卡死。这里 provider 首次调用即抛（模拟预算耗尽），确定性验证
// MethodWriter 的 catch 转换路径。
struct ThrowingProvider : ICommonSuperClassProvider
{
    int callCount = 0;
    ca::str::Utf8StringRef getCommonSuperClass(const ca::str::Utf8StringRef&,
                                               const ca::str::Utf8StringRef&) override
    {
        ++callCount;
        throw CommonSuperClassBudgetExceeded(callCount, 1);
    }
};

TEST(ComputeFramesTest, computeFramesConvertsBudgetExceededToFrameComputationException)
{
    ThrowingProvider inner;
    BoundedCommonSuperClassProvider bounded(&inner, 4);

    CwBuilder cb(Opcodes::V1_7, ref("A"));
    cb.cw.setTypeResolver(&bounded);

    // f(Z)Ljava/lang/Object; —— 两个分支（String 与 Object）在 L2 汇合触发 merge。
    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("f"),
                   ref("(Z)Ljava/lang/Object;"));
    Label* l1 = new Label();
    Label* l2 = new Label();
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitJumpInsn(Opcodes::IFNE, l1);
    cb.mv->visitLdcInsn(ref("x"));                    // String 分支
    cb.mv->visitJumpInsn(Opcodes::GOTO, l2);
    cb.mv->visitLabel(l1);                            // Object 分支
    cb.mv->visitTypeInsn(Opcodes::NEW, ref("java/lang/Object"));
    cb.mv->visitInsn(Opcodes::DUP);
    cb.mv->visitMethodInsn(Opcodes::INVOKESPECIAL, ref("java/lang/Object"), ref("<init>"),
                           ref("()V"), false);
    cb.mv->visitLabel(l2);                            // 汇合：String vs Object → merge
    cb.mv->visitInsn(Opcodes::ARETURN);

    // visitMaxs（endMethod）即触发 computeFrames，预算异常在此转换抛出。
    EXPECT_THROW({ cb.endMethod(); }, FrameComputationException);
    EXPECT_GT(inner.callCount, 0);
}

// #575 崩溃复现：blockShuffle 重排产物——GOTO 跳转到指令流中从未 visitLabel 的
// label（L2 丢失）。MethodWriter 不得崩溃（修复前此测试段错误）。
TEST(ComputeFramesTest, jumpToUnvisitedLabelDoesNotCrash)
{
    CwBuilder cb(Opcodes::V1_7, ref("A"));
    cb.cw.setTypeResolver(&defaultProvider);

    cb.beginMethod(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("f"),
                   ref("()V"));
    Label* l1 = new Label();
    Label* l2 = new Label();
    cb.mv->visitInsn(Opcodes::ICONST_0);
    cb.mv->visitJumpInsn(Opcodes::IFNE, l1);
    cb.mv->visitTypeInsn(Opcodes::NEW, ref("B"));
    cb.mv->visitInsn(Opcodes::DUP);
    cb.mv->visitMethodInsn(Opcodes::INVOKESPECIAL, ref("B"), ref("<init>"), ref("()V"), false);
    cb.mv->visitVarInsn(Opcodes::ASTORE, 1);
    cb.mv->visitJumpInsn(Opcodes::GOTO, l1);
    cb.mv->visitLabel(l1);
    cb.mv->visitJumpInsn(Opcodes::GOTO, l2);   // l2 从未 visitLabel
    cb.mv->visitTypeInsn(Opcodes::NEW, ref("A"));
    cb.mv->visitInsn(Opcodes::DUP);
    cb.mv->visitMethodInsn(Opcodes::INVOKESPECIAL, ref("A"), ref("<init>"), ref("()V"), false);
    cb.mv->visitVarInsn(Opcodes::ASTORE, 1);
    cb.mv->visitVarInsn(Opcodes::ALOAD, 1);
    cb.mv->visitInsn(Opcodes::POP);
    cb.mv->visitInsn(Opcodes::ARETURN);

    // 不崩溃即可（结果为抛出异常或正常产出均可接受，但不得 AV）。
    cb.endMethod();
    cb.cw.visitEnd();
    (void)cb.toBytes();
}



// Test read-back with COMPUTE_FRAMES roundtrip
TEST(ComputeFramesTest, asmAllFramesStructural)
{
    CwBuilder cb(50 << 16 | Opcodes::V1_7, ref("A"));
    // Read a real class, write with COMPUTE_FRAMES, read back
    auto classData = test::getResource("origin/jdk8/AllInstructions.class");
    ASSERT_FALSE(classData.empty())
        << "Missing required test resource: origin/jdk8/AllInstructions.class";

    ClassReader reader(asmArena, classData.data(), 0, static_cast<ca::i32>(classData.size()));
    ClassWriter cw(asmArena, ClassWriter::COMPUTE_FRAMES);
    reader.accept(&cw, 0);
    auto out = cw.toByteArray();
    ASSERT_GT(out.size(), static_cast<size_t>(20));
    uint32_t magic = (static_cast<uint32_t>(out[0]) << 24) | (static_cast<uint32_t>(out[1]) << 16) |
                     (static_cast<uint32_t>(out[2]) << 8) | static_cast<uint32_t>(out[3]);
    ASSERT_EQ(0xCAFEBABE, magic);
}
