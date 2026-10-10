#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "jbytecode/core/exceptions.hpp"
#include "jbytecode/core/readers.hpp"
#include "jbytecode/core/writers.hpp"

using namespace jbytecode::core;

namespace {

ca::str::ZUtf8StringRef ref(const char* value)
{
    return ca::str::ZUtf8StringRef::from_static(value);
}

struct MethodMetrics
{
    int maxStack   = -1;
    int maxLocals  = -1;
    int frameCount = 0;
};

class MetricsMethodVisitor final : public MethodVisitor
{
public:
    explicit MetricsMethodVisitor(MethodMetrics& metrics)
        : MethodVisitor(Opcodes::ASM9)
        , metrics_(metrics)
    {}

    void visitFrame(int, int, const std::vector<ca::core::Any>&, int,
                    const std::vector<ca::core::Any>&) override
    {
        ++metrics_.frameCount;
    }

    void visitMaxs(int maxStack, int maxLocals) override
    {
        metrics_.maxStack  = maxStack;
        metrics_.maxLocals = maxLocals;
    }

private:
    MethodMetrics& metrics_;
};

class MetricsClassVisitor final : public ClassVisitor
{
public:
    MetricsClassVisitor()
        : ClassVisitor(Opcodes::ASM9)
        , methodVisitor_(metrics)
    {}

    MethodVisitor* visitMethod(int, ca::str::Utf8StringRef name, ca::str::Utf8StringRef,
                               ca::str::Utf8StringRef,
                               const std::vector<ca::str::Utf8StringRef>&) override
    {
        return name == ref("test") || name == ref("<init>") ? &methodVisitor_ : nullptr;
    }

    MethodMetrics metrics;

private:
    MetricsMethodVisitor methodVisitor_;
};

MethodMetrics readMetrics(const std::vector<ca::u8>& bytes)
{
    ca::str::Utf8StringArena arena;
    ClassReader              reader(arena, bytes);
    MetricsClassVisitor      visitor;
    reader.accept(&visitor, ClassReader::EXPAND_FRAMES);
    return visitor.metrics;
}

class StressClass
{
public:
    explicit StressClass(int flags, int classAccess = Opcodes::ACC_PUBLIC)
        : writer_(arena_, flags)
    {
        // V7 以后 COMPUTE_MAXS 会从调用方提供的帧推导峰值；纯 CFG 压力测试使用 V1_6。
        const int version =
            (flags & ClassWriter::COMPUTE_FRAMES) != 0 ? Opcodes::V17 : Opcodes::V1_6;
        writer_.visit(
            version, classAccess, ref("stress/Generated"), {}, ref("java/lang/Object"), {});
    }

    MethodVisitor& begin(int access, ca::str::Utf8StringRef descriptor,
                         ca::str::Utf8StringRef name = ref("test"))
    {
        method_ = writer_.visitMethod(access, name, descriptor, {}, {});
        method_->visitCode();
        return *method_;
    }

    Label* label()
    {
        labels_.push_back(std::make_unique<Label>());
        return labels_.back().get();
    }

    std::vector<Label*> labels(int count)
    {
        std::vector<Label*> result;
        result.reserve(static_cast<ca::usize>(count));
        for (int index = 0; index < count; ++index) result.push_back(label());
        return result;
    }

    std::vector<ca::u8> finish()
    {
        method_->visitMaxs(0, 0);
        method_->visitEnd();
        writer_.visitEnd();
        return writer_.toByteArray();
    }

private:
    ca::str::Utf8StringArena            arena_;
    ClassWriter                         writer_;
    MethodVisitor*                      method_ = nullptr;
    std::vector<std::unique_ptr<Label>> labels_;
};

void emitTableSwitch(StressClass& generated, MethodVisitor& method, int caseCount)
{
    auto  cases       = generated.labels(caseCount);
    auto* defaultCase = generated.label();
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitTableSwitchInsn(0, caseCount - 1, defaultCase, cases);
    for (auto* label : cases) {
        method.visitLabel(label);
        method.visitInsn(Opcodes::ICONST_0);
        method.visitInsn(Opcodes::IRETURN);
    }
    method.visitLabel(defaultCase);
    method.visitInsn(Opcodes::ICONST_M1);
    method.visitInsn(Opcodes::IRETURN);
}

void emitLookupSwitch(StressClass& generated, MethodVisitor& method, int caseCount)
{
    auto             cases = generated.labels(caseCount);
    std::vector<int> keys;
    keys.reserve(static_cast<ca::usize>(caseCount));
    for (int index = 0; index < caseCount; ++index) keys.push_back(index * 97 + 3);
    auto* defaultCase = generated.label();
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitLookupSwitchInsn(defaultCase, keys, cases);
    for (auto* label : cases) {
        method.visitLabel(label);
        method.visitInsn(Opcodes::ICONST_1);
        method.visitInsn(Opcodes::IRETURN);
    }
    method.visitLabel(defaultCase);
    method.visitInsn(Opcodes::ICONST_M1);
    method.visitInsn(Opcodes::IRETURN);
}

void expectReadableWithFrames(const std::vector<ca::u8>& bytes, int minimumFrames = 1)
{
    const auto metrics = readMetrics(bytes);
    EXPECT_GE(metrics.maxStack, 0);
    EXPECT_GE(metrics.maxLocals, 0);
    EXPECT_GE(metrics.frameCount, minimumFrames);
}

TEST(ComputeMaxsStressTest, LargeLinearMethodWithTenThousandInstructions)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    for (int index = 0; index < 10000; ++index) method.visitInsn(Opcodes::NOP);
    method.visitInsn(Opcodes::RETURN);

    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(0, metrics.maxStack);
    EXPECT_EQ(0, metrics.maxLocals);
}

TEST(ComputeMaxsStressTest, DeepSingleSlotOperandStack)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    for (int index = 0; index < 128; ++index) method.visitInsn(Opcodes::ICONST_0);
    for (int index = 0; index < 128; ++index) method.visitInsn(Opcodes::POP);
    method.visitInsn(Opcodes::RETURN);

    EXPECT_EQ(128, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, DeepWideOperandStackCountsSlots)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    for (int index = 0; index < 64; ++index) method.visitInsn(Opcodes::LCONST_0);
    for (int index = 0; index < 64; ++index) method.visitInsn(Opcodes::POP2);
    method.visitInsn(Opcodes::RETURN);

    EXPECT_EQ(128, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, HighIntegerLocalIndex)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    method.visitInsn(Opcodes::ICONST_0);
    method.visitVarInsn(Opcodes::ISTORE, 150);
    method.visitInsn(Opcodes::RETURN);

    EXPECT_EQ(151, readMetrics(generated.finish()).maxLocals);
}

TEST(ComputeMaxsStressTest, WideLocalsReserveTwoSlots)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    method.visitInsn(Opcodes::LCONST_0);
    method.visitVarInsn(Opcodes::LSTORE, 150);
    method.visitInsn(Opcodes::DCONST_0);
    method.visitVarInsn(Opcodes::DSTORE, 200);
    method.visitInsn(Opcodes::RETURN);

    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(2, metrics.maxStack);
    EXPECT_EQ(202, metrics.maxLocals);
}

TEST(ComputeMaxsStressTest, DescriptorWideArgumentsSeedLocals)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method =
        generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(IDJLjava/lang/Object;)V"));
    method.visitInsn(Opcodes::RETURN);

    EXPECT_EQ(6, readMetrics(generated.finish()).maxLocals);
}

TEST(ComputeMaxsStressTest, SixtyFourForwardBranches)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method  = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)I"));
    auto        targets = generated.labels(64);
    for (auto* target : targets) {
        method.visitVarInsn(Opcodes::ILOAD, 0);
        method.visitJumpInsn(Opcodes::IFEQ, target);
    }
    method.visitInsn(Opcodes::ICONST_1);
    method.visitInsn(Opcodes::IRETURN);
    for (auto* target : targets) {
        method.visitLabel(target);
        method.visitInsn(Opcodes::ICONST_0);
        method.visitInsn(Opcodes::IRETURN);
    }

    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(1, metrics.maxStack);
    EXPECT_EQ(1, metrics.maxLocals);
}

TEST(ComputeMaxsStressTest, BackwardLoop)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)V"));
    auto*       loop   = generated.label();
    auto*       done   = generated.label();
    method.visitLabel(loop);
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, done);
    method.visitIincInsn(0, -1);
    method.visitJumpInsn(Opcodes::GOTO, loop);
    method.visitLabel(done);
    method.visitInsn(Opcodes::RETURN);

    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(1, metrics.maxStack);
    EXPECT_EQ(1, metrics.maxLocals);
}

TEST(ComputeMaxsStressTest, LargeTableSwitch)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)I"));
    emitTableSwitch(generated, method, 128);
    EXPECT_EQ(1, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, LargeLookupSwitch)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)I"));
    emitLookupSwitch(generated, method, 128);
    EXPECT_EQ(1, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, ManyTryCatchRangesShareHandler)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method  = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    auto*       handler = generated.label();
    for (int index = 0; index < 32; ++index) {
        auto* start = generated.label();
        auto* end   = generated.label();
        method.visitTryCatchBlock(start, end, handler, ref("java/lang/Exception"));
        method.visitLabel(start);
        method.visitInsn(Opcodes::NOP);
        method.visitLabel(end);
    }
    method.visitInsn(Opcodes::RETURN);
    method.visitLabel(handler);
    method.visitInsn(Opcodes::POP);
    method.visitInsn(Opcodes::RETURN);

    EXPECT_EQ(1, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, MixedArrayOperationsReachFourSlots)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()I"));
    method.visitInsn(Opcodes::ICONST_4);
    method.visitIntInsn(Opcodes::NEWARRAY, Opcodes::T_INT);
    method.visitInsn(Opcodes::DUP);
    method.visitInsn(Opcodes::ICONST_0);
    method.visitIntInsn(Opcodes::BIPUSH, 7);
    method.visitInsn(Opcodes::IASTORE);
    method.visitInsn(Opcodes::ARRAYLENGTH);
    method.visitInsn(Opcodes::IRETURN);

    EXPECT_EQ(4, readMetrics(generated.finish()).maxStack);
}

TEST(ComputeMaxsStressTest, LargeMethodBelowCodeLimit)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    for (int index = 0; index < 60000; ++index) method.visitInsn(Opcodes::NOP);
    method.visitInsn(Opcodes::RETURN);
    EXPECT_NO_THROW((void)generated.finish());
}

TEST(ComputeMaxsStressTest, MethodAboveCodeLimitIsRejected)
{
    StressClass generated(ClassWriter::COMPUTE_MAXS);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    for (int index = 0; index < 70000; ++index) method.visitInsn(Opcodes::NOP);
    method.visitInsn(Opcodes::RETURN);
    EXPECT_THROW((void)generated.finish(), MethodTooLargeException);
}

TEST(ComputeFramesStressTest, WhileLoopFramesRoundtrip)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)V"));
    auto*       loop   = generated.label();
    auto*       done   = generated.label();
    method.visitLabel(loop);
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, done);
    method.visitIincInsn(0, -1);
    method.visitJumpInsn(Opcodes::GOTO, loop);
    method.visitLabel(done);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish());
}

TEST(ComputeFramesStressTest, DoWhileLoopFramesRoundtrip)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)V"));
    auto*       loop   = generated.label();
    method.visitLabel(loop);
    method.visitIincInsn(0, -1);
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFGT, loop);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish(), 0);
}

TEST(ComputeFramesStressTest, EightNestedLoopsRoundtrip)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    auto        starts = generated.labels(8);
    auto        ends   = generated.labels(8);
    for (int depth = 0; depth < 8; ++depth) {
        method.visitInsn(Opcodes::ICONST_2);
        method.visitVarInsn(Opcodes::ISTORE, depth);
        method.visitLabel(starts[depth]);
        method.visitVarInsn(Opcodes::ILOAD, depth);
        method.visitJumpInsn(Opcodes::IFEQ, ends[depth]);
    }
    for (int depth = 7; depth >= 0; --depth) {
        method.visitIincInsn(depth, -1);
        method.visitJumpInsn(Opcodes::GOTO, starts[depth]);
        method.visitLabel(ends[depth]);
    }
    method.visitInsn(Opcodes::RETURN);
    const auto bytes = generated.finish();
    EXPECT_GE(readMetrics(bytes).maxLocals, 8);
    expectReadableWithFrames(bytes, 16);
}

TEST(ComputeFramesStressTest, LoopWithInternalTryCatch)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method  = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)V"));
    auto*       loop    = generated.label();
    auto*       start   = generated.label();
    auto*       end     = generated.label();
    auto*       handler = generated.label();
    auto*       done    = generated.label();
    method.visitTryCatchBlock(start, end, handler, ref("java/lang/RuntimeException"));
    method.visitLabel(loop);
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, done);
    method.visitLabel(start);
    method.visitIincInsn(0, -1);
    method.visitLabel(end);
    method.visitJumpInsn(Opcodes::GOTO, loop);
    method.visitLabel(handler);
    method.visitInsn(Opcodes::POP);
    method.visitJumpInsn(Opcodes::GOTO, loop);
    method.visitLabel(done);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish(), 2);
}

TEST(ComputeFramesStressTest, FinallyStyleHandlerRethrows)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method  = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    auto*       start   = generated.label();
    auto*       end     = generated.label();
    auto*       handler = generated.label();
    auto*       done    = generated.label();
    method.visitTryCatchBlock(start, end, handler, {});
    method.visitLabel(start);
    method.visitInsn(Opcodes::NOP);
    method.visitLabel(end);
    method.visitJumpInsn(Opcodes::GOTO, done);
    method.visitLabel(handler);
    method.visitVarInsn(Opcodes::ASTORE, 0);
    method.visitInsn(Opcodes::NOP);
    method.visitVarInsn(Opcodes::ALOAD, 0);
    method.visitInsn(Opcodes::ATHROW);
    method.visitLabel(done);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish(), 2);
}

TEST(ComputeFramesStressTest, ConstructorInitializationChain)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC, ref("()V"), ref("<init>"));
    method.visitVarInsn(Opcodes::ALOAD, 0);
    method.visitMethodInsn(
        Opcodes::INVOKESPECIAL, ref("java/lang/Object"), ref("<init>"), ref("()V"), false);
    method.visitInsn(Opcodes::RETURN);
    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(1, metrics.maxLocals);
    EXPECT_EQ(1, metrics.maxStack);
}

TEST(ComputeFramesStressTest, InterfaceDefaultMethodHasComputedLimits)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES,
                          Opcodes::ACC_PUBLIC | Opcodes::ACC_INTERFACE | Opcodes::ACC_ABSTRACT);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC, ref("()I"));
    method.visitInsn(Opcodes::ICONST_1);
    method.visitInsn(Opcodes::IRETURN);

    const auto metrics = readMetrics(generated.finish());
    EXPECT_EQ(1, metrics.maxLocals);
    EXPECT_EQ(1, metrics.maxStack);
}

TEST(ComputeFramesStressTest, NewObjectRemainsInitializedAfterJoin)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method =
        generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(Z)Ljava/lang/Object;"));
    auto* other = generated.label();
    auto* join  = generated.label();
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, other);
    method.visitTypeInsn(Opcodes::NEW, ref("java/lang/StringBuilder"));
    method.visitInsn(Opcodes::DUP);
    method.visitMethodInsn(
        Opcodes::INVOKESPECIAL, ref("java/lang/StringBuilder"), ref("<init>"), ref("()V"), false);
    method.visitJumpInsn(Opcodes::GOTO, join);
    method.visitLabel(other);
    method.visitLdcInsn(std::string("text"));
    method.visitLabel(join);
    method.visitInsn(Opcodes::ARETURN);
    expectReadableWithFrames(generated.finish(), 2);
}

TEST(ComputeFramesStressTest, LongValuesMergeAcrossBranch)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(Z)J"));
    auto*       other  = generated.label();
    auto*       join   = generated.label();
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, other);
    method.visitInsn(Opcodes::LCONST_0);
    method.visitJumpInsn(Opcodes::GOTO, join);
    method.visitLabel(other);
    method.visitInsn(Opcodes::LCONST_1);
    method.visitLabel(join);
    method.visitInsn(Opcodes::LRETURN);
    const auto bytes = generated.finish();
    EXPECT_EQ(2, readMetrics(bytes).maxStack);
    expectReadableWithFrames(bytes, 2);
}

TEST(ComputeFramesStressTest, DoubleValuesMergeAcrossBranch)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(Z)D"));
    auto*       other  = generated.label();
    auto*       join   = generated.label();
    method.visitVarInsn(Opcodes::ILOAD, 0);
    method.visitJumpInsn(Opcodes::IFEQ, other);
    method.visitInsn(Opcodes::DCONST_0);
    method.visitJumpInsn(Opcodes::GOTO, join);
    method.visitLabel(other);
    method.visitInsn(Opcodes::DCONST_1);
    method.visitLabel(join);
    method.visitInsn(Opcodes::DRETURN);
    const auto bytes = generated.finish();
    EXPECT_EQ(2, readMetrics(bytes).maxStack);
    expectReadableWithFrames(bytes, 2);
}

TEST(ComputeFramesStressTest, DeadCodeDoesNotBreakFrameEmission)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    auto*       live   = generated.label();
    method.visitJumpInsn(Opcodes::GOTO, live);
    method.visitInsn(Opcodes::ICONST_0);
    method.visitInsn(Opcodes::POP);
    method.visitInsn(Opcodes::RETURN);
    method.visitLabel(live);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish());
}

TEST(ComputeFramesStressTest, TenThousandInstructionMethodWithBranch)
{
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("()V"));
    auto*       done   = generated.label();
    method.visitInsn(Opcodes::ICONST_0);
    method.visitJumpInsn(Opcodes::IFEQ, done);
    for (int index = 0; index < 10000; ++index) method.visitInsn(Opcodes::NOP);
    method.visitLabel(done);
    method.visitInsn(Opcodes::RETURN);
    expectReadableWithFrames(generated.finish());
}

class SwitchFrameStressTest : public ::testing::TestWithParam<std::pair<bool, int>>
{};

TEST_P(SwitchFrameStressTest, LargeSwitchFramesRoundtrip)
{
    const auto [lookup, count] = GetParam();
    StressClass generated(ClassWriter::COMPUTE_FRAMES);
    auto&       method = generated.begin(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("(I)I"));
    if (lookup)
        emitLookupSwitch(generated, method, count);
    else
        emitTableSwitch(generated, method, count);
    expectReadableWithFrames(generated.finish(), count);
}

INSTANTIATE_TEST_SUITE_P(TableAndLookupSizes, SwitchFrameStressTest,
                         ::testing::Values(std::pair{false, 4}, std::pair{false, 32},
                                           std::pair{false, 128}, std::pair{true, 4},
                                           std::pair{true, 32}, std::pair{true, 128}));

}   // namespace
TEST(ComputeStressTest, rejectsOpcodesOutsideStackDeltaTable)
{
    ca::str::Utf8StringArena arena;
    ClassWriter              writer(arena, ClassWriter::COMPUTE_MAXS);
    writer.visit(Opcodes::V1_8,
                 Opcodes::ACC_PUBLIC,
                 ref("InvalidOpcode"),
                 ref(""),
                 ref("java/lang/Object"),
                 {});
    MethodVisitor* method = writer.visitMethod(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("test"), ref("()V"), ref(""), {});
    ASSERT_NE(method, nullptr);
    Label target;

    EXPECT_THROW(method->visitInsn(-1), std::invalid_argument);
    EXPECT_THROW(method->visitInsn(202), std::invalid_argument);
    EXPECT_THROW(method->visitVarInsn(-1, 0), std::invalid_argument);
    EXPECT_THROW(method->visitVarInsn(202, 0), std::invalid_argument);
    EXPECT_THROW(method->visitJumpInsn(-1, &target), std::invalid_argument);
    EXPECT_THROW(method->visitJumpInsn(400, &target), std::invalid_argument);
}
