#include <gtest/gtest.h>
#include <cstring>
#include <stdexcept>
#include "test_utils.hpp"

namespace {

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
    std::vector<Insn> recorded;

    void visitInsn(int opcode) override { recorded.push_back({0, opcode, 0}); }
    void visitIntInsn(int opcode, int operand) override
    {
        recorded.push_back({1, opcode, operand});
    }
    void visitLdcInsn(const ca::core::Any&) override { recorded.push_back({2, 0, 0}); }
    void visitVarInsn(int opcode, int var) override { recorded.push_back({3, opcode, var}); }
    void visitMaxs(int, int) override {}
};

struct InvokeRecorder : jbytecode::core::MethodVisitor
{
    InvokeRecorder()
        : jbytecode::core::MethodVisitor(Opcodes::ASM9, nullptr)
    {}
    std::vector<int> invokedOpcodes;

    void visitInsn(int opcode) override { invokedOpcodes.push_back(opcode); }
    void visitIntInsn(int opcode, int operand) override { invokedOpcodes.push_back(opcode); }
    void visitLdcInsn(const ca::core::Any&) override { invokedOpcodes.push_back(-1); }
    void visitVarInsn(int opcode, int var) override { invokedOpcodes.push_back(opcode); }
    void visitFieldInsn(int opcode, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                        ca::str::Utf8StringRef) override
    {
        invokedOpcodes.push_back(opcode);
    }
    void visitMethodInsn(int opcode, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
                         ca::str::Utf8StringRef, bool) override
    {
        invokedOpcodes.push_back(opcode);
    }
    void visitTypeInsn(int opcode, ca::str::Utf8StringRef) override
    {
        invokedOpcodes.push_back(opcode);
    }
    void visitJumpInsn(int opcode, jbytecode::core::Label*) override
    {
        invokedOpcodes.push_back(opcode);
    }
    void visitMaxs(int, int) override {}
};

}   // namespace

TEST(GeneratorAdapterTest, testConstructor_emptyDescriptor)
{
    RecordingVisitor rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_PUBLIC,
                        ZUtf8StringRef::from_static("test"),
                        ZUtf8StringRef::from_static("()V"));
    EXPECT_EQ(Opcodes::ACC_PUBLIC, ga.getAccess());
    EXPECT_EQ(ZUtf8StringRef::from_static("test"), ga.getName());
    EXPECT_EQ(Type::VOID_TYPE, ga.getReturnType());
    EXPECT_TRUE(ga.getArgumentTypes().empty());
}

TEST(GeneratorAdapterTest, testConstructor_basicDescriptor)
{
    RecordingVisitor rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_PRIVATE,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("(I)F"));
    EXPECT_EQ(Opcodes::ACC_PRIVATE, ga.getAccess());
    EXPECT_EQ(ZUtf8StringRef::from_static("m"), ga.getName());
    EXPECT_EQ(Type::FLOAT_TYPE, ga.getReturnType());
    auto args = ga.getArgumentTypes();
    ASSERT_EQ(1u, args.size());
    EXPECT_EQ(Type::INT_TYPE, args[0]);
}

TEST(GeneratorAdapterTest, testPush_int_iconstRange)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.push(-1);
    ga.push(0);
    ga.push(5);
    ASSERT_EQ(3u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::ICONST_M1, rec.invokedOpcodes[0]);
    EXPECT_EQ(Opcodes::ICONST_0, rec.invokedOpcodes[1]);
    EXPECT_EQ(Opcodes::ICONST_0 + 5, rec.invokedOpcodes[2]);
}

TEST(GeneratorAdapterTest, testPush_int_bipushRange)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.push(100);
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::BIPUSH, rec.invokedOpcodes[0]);
}

TEST(GeneratorAdapterTest, testPush_int_sipushRange)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.push(1000);
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::SIPUSH, rec.invokedOpcodes[0]);
}

TEST(GeneratorAdapterTest, testPush_int_ldcRange)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.push(100000);
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(-1, rec.invokedOpcodes[0]);   // visitLdcInsn
}

TEST(GeneratorAdapterTest, testPush_boolean)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.push(false);
    ga.push(true);
    ASSERT_EQ(2u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::ICONST_0, rec.invokedOpcodes[0]);
    EXPECT_EQ(Opcodes::ICONST_1, rec.invokedOpcodes[1]);
}

TEST(GeneratorAdapterTest, testPop)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.pop();
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::POP, rec.invokedOpcodes[0]);
}

TEST(GeneratorAdapterTest, testDup)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.dup();
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::DUP, rec.invokedOpcodes[0]);
}

TEST(GeneratorAdapterTest, testLoadThis_instance)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_PUBLIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    ga.loadThis();
    ASSERT_EQ(1u, rec.invokedOpcodes.size());
    EXPECT_EQ(Opcodes::ALOAD, rec.invokedOpcodes[0]);
}

TEST(GeneratorAdapterTest, testLoadThis_staticThrows)
{
    InvokeRecorder   rec;
    GeneratorAdapter ga(&rec,
                        Opcodes::ACC_STATIC,
                        ZUtf8StringRef::from_static("m"),
                        ZUtf8StringRef::from_static("()V"));
    EXPECT_THROW(ga.loadThis(), std::logic_error);
}

namespace {

/// #1068 R19-F1 红验访客：在 visitMethod 内把 exceptions 内部名的字节与
/// 地址哨兵留档。旧实现（#921 批次2）里 ARRAY 排序项的 ref 指向
/// getInternalNames 函数级局部 scratch arena，本访客被调时该 arena 已析构，
/// 读到的已是释放内存（ASan: heap-use-after-free；无 ASan debug 堆为 0xDD 死区填充）。
struct ExceptionsRecordingClassVisitor : jbytecode::core::ClassVisitor
{
    ExceptionsRecordingClassVisitor()
        : jbytecode::core::ClassVisitor(Opcodes::ASM9, nullptr)
    {}

    std::vector<std::vector<ca::u8>> exceptionBytes;
    std::vector<const ca::u8*>       rawPtrs;   // 地址哨兵：ref 指向的原始内存

    jbytecode::core::MethodVisitor* visitMethod(
        int, ca::str::Utf8StringRef, ca::str::Utf8StringRef, ca::str::Utf8StringRef,
        const std::vector<ca::str::Utf8StringRef>& exceptions) override
    {
        for (const auto& e : exceptions) {
            exceptionBytes.emplace_back(e.data(), e.data() + e.byte_length());
            rawPtrs.push_back(e.data());
        }
        return nullptr;
    }
};

std::vector<ca::u8> toBytes(const char* s)
{
    return std::vector<ca::u8>(reinterpret_cast<const ca::u8*>(s),
                               reinterpret_cast<const ca::u8*>(s) + std::strlen(s));
}

}   // namespace

/// #1068 R19-F1：5 参构造函数对 ARRAY 排序 exceptions 曾把 internal name
/// intern 进 getInternalNames 的函数级局部 scratch，visitMethod 在 scratch
/// 死后消费悬垂 ref。合法 JVM 异常恒为 class 排序（当前不可达），但本构造
/// 函数是 public API，接受任意 Type 向量——用 ARRAY 排序输入钉住该分支：
/// visitMethod 时读到的字节必须与期望内部名逐字节一致（修复前 ASan 红）。
TEST(GeneratorAdapterTest, testConstructor_arraySortExceptionInternalName_notDangling)
{
    const char* kObjectName = "java/lang/Exception";
    const char* kArrayName  = "[Ljava/lang/Exception;";

    std::vector<Type> exceptions{Type::getObjectType(ZUtf8StringRef::from_static(kObjectName)),
                                 Type::getType(ZUtf8StringRef::from_static(kArrayName))};
    ASSERT_EQ(Type::OBJECT, exceptions[0].getSort());
    ASSERT_EQ(Type::ARRAY, exceptions[1].getSort());

    ExceptionsRecordingClassVisitor cv;
    const Method method(ZUtf8StringRef::from_static("run"), Type::VOID_TYPE,
                        std::vector<Type>{});
    GeneratorAdapter ga(Opcodes::ACC_PUBLIC, method,
                        ZUtf8StringRef::from_static("()Ljava/lang/Object;"), exceptions, &cv);

    ASSERT_EQ(2u, cv.exceptionBytes.size());
    EXPECT_EQ(toBytes(kObjectName), cv.exceptionBytes[0]);
    EXPECT_EQ(toBytes(kArrayName), cv.exceptionBytes[1]);
    // 地址哨兵非空（空视图/intern 失败会退化为空串，须显式区分）。
    ASSERT_NE(nullptr, cv.rawPtrs[0]);
    ASSERT_NE(nullptr, cv.rawPtrs[1]);
}
