#include "garena/arena.hpp"
#include <gtest/gtest.h>

#include <vector>

#include "jbytecode/analysis/analyzer.hpp"
#include "jbytecode/analysis/basic_interpreter.hpp"
#include "jbytecode/analysis/basic_verifier.hpp"
#include "jbytecode/analysis/simple_verifier.hpp"
#include "jbytecode/analysis/source_interpreter.hpp"

// B8b：测试侧 Label 统一归宿（进程寿命 scratch arena）——LabelNode() 默认构造
// 已随旧池制度退役，Label 一律 arena 分配。
static garena::Arena& labelScratchArena()
{
    static garena::Arena arena;
    return arena;
}


using namespace jbytecode::analysis;
using namespace jbytecode::core;
using namespace jbytecode::tree;

namespace {

ca::str::ZUtf8StringRef ref(const char* value)
{
    return ca::str::ZUtf8StringRef::from_static(value);
}

class ExposedSimpleVerifier final : public SimpleVerifier
{
public:
    using SimpleVerifier::SimpleVerifier;

    BasicValue mergeValues(const BasicValue& first, const BasicValue& second)
    {
        return merge(first, second);
    }
};

TEST(BasicInterpreterBoundaryStressTest, WideConstantsProduceTwoSlotValues)
{
    BasicInterpreter interpreter;
    InsnNode         longConstant(Opcodes::LCONST_0);
    InsnNode         doubleConstant(Opcodes::DCONST_0);
    EXPECT_EQ(2, interpreter.newOperation(&longConstant).getSize());
    EXPECT_EQ(2, interpreter.newOperation(&doubleConstant).getSize());
}

TEST(BasicInterpreterBoundaryStressTest, NewOperationProducesReference)
{
    BasicInterpreter interpreter;
    TypeInsnNode     instruction(Opcodes::NEW, ref("java/lang/StringBuilder"));
    EXPECT_TRUE(interpreter.newOperation(&instruction).isReference());
}

TEST(BasicInterpreterBoundaryStressTest, UnaryConversionsProduceWideValues)
{
    BasicInterpreter interpreter;
    InsnNode         intToLong(Opcodes::I2L);
    InsnNode         intToDouble(Opcodes::I2D);
    EXPECT_EQ(BasicValue::LONG_VALUE,
              interpreter.unaryOperation(&intToLong, BasicValue::INT_VALUE));
    EXPECT_EQ(BasicValue::DOUBLE_VALUE,
              interpreter.unaryOperation(&intToDouble, BasicValue::INT_VALUE));
}

TEST(BasicInterpreterBoundaryStressTest, WideComparisonsProduceInt)
{
    BasicInterpreter interpreter;
    InsnNode         longCompare(Opcodes::LCMP);
    InsnNode         doubleCompare(Opcodes::DCMPG);
    EXPECT_EQ(
        BasicValue::INT_VALUE,
        interpreter.binaryOperation(&longCompare, BasicValue::LONG_VALUE, BasicValue::LONG_VALUE));
    EXPECT_EQ(BasicValue::INT_VALUE,
              interpreter.binaryOperation(
                  &doubleCompare, BasicValue::DOUBLE_VALUE, BasicValue::DOUBLE_VALUE));
}

TEST(BasicInterpreterBoundaryStressTest, MultiArrayNaryOperationProducesReference)
{
    BasicInterpreter        interpreter;
    MultiANewArrayInsnNode  instruction(ref("[[[I"), 3);
    std::vector<BasicValue> dimensions(3, BasicValue::INT_VALUE);
    EXPECT_TRUE(interpreter.naryOperation(&instruction, dimensions).isReference());
}

TEST(BasicInterpreterBoundaryStressTest, ArrayStoreTernaryOperationProducesNoValue)
{
    BasicInterpreter interpreter;
    InsnNode         instruction(Opcodes::IASTORE);
    const auto       result = interpreter.ternaryOperation(
        &instruction, BasicValue::REFERENCE_VALUE, BasicValue::INT_VALUE, BasicValue::INT_VALUE);
    EXPECT_EQ(nullptr, result.getType());
}

TEST(BasicVerifierBoundaryStressTest, RejectsReferenceInIntegerArithmetic)
{
    BasicVerifier verifier;
    InsnNode      instruction(Opcodes::IADD);
    EXPECT_THROW(
        verifier.binaryOperation(&instruction, BasicValue::REFERENCE_VALUE, BasicValue::INT_VALUE),
        AnalyzerException);
}

TEST(BasicVerifierBoundaryStressTest, RejectsPrimitiveReferenceReturn)
{
    BasicVerifier verifier;
    InsnNode      instruction(Opcodes::ARETURN);
    EXPECT_THROW(verifier.unaryOperation(&instruction, BasicValue::INT_VALUE), AnalyzerException);
}

TEST(BasicVerifierBoundaryStressTest, RejectsReferenceArrayIndex)
{
    BasicVerifier verifier;
    InsnNode      instruction(Opcodes::IALOAD);
    EXPECT_THROW(verifier.binaryOperation(
                     &instruction, BasicValue::REFERENCE_VALUE, BasicValue::REFERENCE_VALUE),
                 AnalyzerException);
}

TEST(AnalyzerBoundaryStressTest, ExceptionHandlerReceivesOneReference)
{
    MethodNode method(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("guarded"), ref("()V"), {}, {});
    method.maxStack  = 1;
    method.maxLocals = 1;
    auto* start      = new LabelNode(labelScratchArena().create<Label>());
    auto* end        = new LabelNode(labelScratchArena().create<Label>());
    auto* handler    = new LabelNode(labelScratchArena().create<Label>());
    method.instructions.add(start);
    method.instructions.add(new InsnNode(Opcodes::ACONST_NULL));
    method.instructions.add(new InsnNode(Opcodes::ATHROW));
    method.instructions.add(end);
    method.instructions.add(handler);
    method.instructions.add(new VarInsnNode(Opcodes::ASTORE, 0));
    method.instructions.add(new InsnNode(Opcodes::RETURN));
    method.tryCatchBlocks.push_back(method.create<TryCatchBlockNode>(
        start->label, end->label, handler->label, ref("java/lang/Exception")));

    BasicInterpreter     interpreter;
    Analyzer<BasicValue> analyzer(&interpreter);
    const auto           frames       = analyzer.analyze(ref("stress/Owner"), &method);
    const int            handlerIndex = method.instructions.indexOf(handler);
    ASSERT_GE(handlerIndex, 0);
    ASSERT_NE(nullptr, frames[handlerIndex]);
    ASSERT_EQ(1, frames[handlerIndex]->getStackSize());
    EXPECT_TRUE(frames[handlerIndex]->getStack(0).isReference());
}

TEST(AnalyzerBoundaryStressTest, DeadCodeHasNoFrame)
{
    MethodNode method(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("dead"), ref("()V"), {}, {});
    method.maxStack  = 0;
    method.maxLocals = 0;
    auto* live       = new LabelNode(labelScratchArena().create<Label>());
    method.instructions.add(new JumpInsnNode(Opcodes::GOTO, live->label));
    method.instructions.add(new InsnNode(Opcodes::NOP));
    method.instructions.add(live);
    method.instructions.add(new InsnNode(Opcodes::RETURN));

    BasicInterpreter     interpreter;
    Analyzer<BasicValue> analyzer(&interpreter);
    const auto           frames = analyzer.analyze(ref("stress/Owner"), &method);
    ASSERT_EQ(4U, frames.size());
    EXPECT_EQ(nullptr, frames[1]);
    EXPECT_NE(nullptr, frames[2]);
}

TEST(AnalyzerBoundaryStressTest, LargeLinearMethodKeepsEveryInstructionReachable)
{
    MethodNode method(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("linear"), ref("()V"), {}, {});
    method.maxStack  = 0;
    method.maxLocals = 0;
    for (int index = 0; index < 2048; ++index) {
        method.instructions.add(new InsnNode(Opcodes::NOP));
    }
    method.instructions.add(new InsnNode(Opcodes::RETURN));

    BasicInterpreter     interpreter;
    Analyzer<BasicValue> analyzer(&interpreter);
    const auto           frames = analyzer.analyze(ref("stress/Owner"), &method);
    ASSERT_EQ(2049U, frames.size());
    for (auto* frame : frames) EXPECT_NE(nullptr, frame);
}

TEST(AnalyzerBoundaryStressTest, TableSwitchMakesEveryTargetReachable)
{
    MethodNode method(
        Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC, ref("select"), ref("(I)I"), {}, {});
    method.maxStack  = 1;
    method.maxLocals = 1;
    auto* first      = new LabelNode(labelScratchArena().create<Label>());
    auto* second     = new LabelNode(labelScratchArena().create<Label>());
    auto* fallback   = new LabelNode(labelScratchArena().create<Label>());
    method.instructions.add(new VarInsnNode(Opcodes::ILOAD, 0));
    method.instructions.add(
        new TableSwitchInsnNode(0, 1, fallback->label, {first->label, second->label}));
    method.instructions.add(first);
    method.instructions.add(new InsnNode(Opcodes::ICONST_0));
    method.instructions.add(new InsnNode(Opcodes::IRETURN));
    method.instructions.add(second);
    method.instructions.add(new InsnNode(Opcodes::ICONST_1));
    method.instructions.add(new InsnNode(Opcodes::IRETURN));
    method.instructions.add(fallback);
    method.instructions.add(new InsnNode(Opcodes::ICONST_M1));
    method.instructions.add(new InsnNode(Opcodes::IRETURN));

    BasicInterpreter     interpreter;
    Analyzer<BasicValue> analyzer(&interpreter);
    const auto           frames = analyzer.analyze(ref("stress/Owner"), &method);
    EXPECT_NE(nullptr, frames[method.instructions.indexOf(first)]);
    EXPECT_NE(nullptr, frames[method.instructions.indexOf(second)]);
    EXPECT_NE(nullptr, frames[method.instructions.indexOf(fallback)]);
}

TEST(SimpleVerifierBoundaryStressTest, ConfiguredHierarchyMergesCurrentClassToSuperClass)
{
    const Type            child = Type::getObjectType(ref("stress/Child"));
    const Type            base  = Type::getObjectType(ref("stress/Base"));
    ExposedSimpleVerifier verifier(child, base, false);

    const auto merged = verifier.mergeValues(verifier.newValue(&child), verifier.newValue(&base));

    ASSERT_NE(nullptr, merged.getType());
    EXPECT_EQ(base, *merged.getType());
}

TEST(SourceInterpreterBoundaryStressTest, MergeRetainsAllDistinctProducers)
{
    SourceInterpreter interpreter;
    InsnNode          first(Opcodes::ICONST_0);
    InsnNode          second(Opcodes::ICONST_1);
    InsnNode          third(Opcodes::ICONST_2);
    auto              merged = interpreter.merge(SourceValue(1, &first), SourceValue(1, &second));
    merged                   = interpreter.merge(merged, SourceValue(1, &third));
    EXPECT_EQ(3U, merged.insns_.size());
}

TEST(SourceInterpreterBoundaryStressTest, WideProducerSizeSurvivesCopy)
{
    SourceInterpreter interpreter;
    InsnNode          producer(Opcodes::LCONST_0);
    InsnNode          copy(Opcodes::DUP2);
    SourceValue       value(2, &producer);
    const auto        copied = interpreter.copyOperation(&copy, value);
    EXPECT_EQ(2, copied.getSize());
    EXPECT_EQ(1U, copied.insns_.size());
}

// 回归 #742：真实编译管线里 STRING 常量以 Utf8StringRef 标签落入 LdcInsnNode
// （见 libjblw conversion.cpp unwrapConstant → OfString::value().ref()），而非 std::string。
// BasicInterpreter::newOperation 的 LDC 分支曾用 cst.is<std::string>() 检查，
// 永远为 false，导致 ldc "字符串" 推断为 UNINITIALIZED_VALUE，紧随的 invokevirtual
// 取 String 引用参数时报 "Expected argument 1 Ljava/lang/String;, but got ."。
// 此处构造 issue 最小复现场景（System.out.println("hi")）并用 BasicVerifier
// 跑完整帧分析——这正是 patch_validate_jasm / method_validate_jasm 的校验路径。
TEST(LdcStringRegression742Test, Utf8StringRefLdcInfersStringReference)
{
    BasicInterpreter interpreter;
    // 与真实管线一致：Utf8StringRef 标签（非 ZUtf8StringRef / std::string）
    LdcInsnNode ldc(ca::str::Utf8StringRef::from_cstr("hi"));
    const auto  value = interpreter.newOperation(&ldc);
    // 修复前：cst.is<std::string>() 恒 false → 返回 UNINITIALIZED_VALUE（getType()==nullptr）。
    // 修复后：命中 Utf8StringRef 分支 → BasicInterpreter::newValue 对 OBJECT sort
    // 折叠为通用 REFERENCE_VALUE（与 SimpleVerifier 不同，不做精确类型区分）。
    ASSERT_NE(nullptr, value.getType());
    EXPECT_TRUE(value.isReference());
}

TEST(LdcStringRegression742Test, LdcStringFollowedByInvokeAnalyzesCleanly)
{
    MethodNode method(Opcodes::ACC_PUBLIC | Opcodes::ACC_STATIC,
                      ref("main"),
                      ref("([Ljava/lang/String;)V"),
                      {},
                      {});
    method.maxStack  = 2;
    method.maxLocals = 1;
    method.instructions.add(new FieldInsnNode(
        Opcodes::GETSTATIC, ref("java/lang/System"), ref("out"), ref("Ljava/io/PrintStream;")));
    method.instructions.add(new LdcInsnNode(ca::str::Utf8StringRef::from_cstr("hi")));
    method.instructions.add(new MethodInsnNode(Opcodes::INVOKEVIRTUAL,
                                               ref("java/io/PrintStream"),
                                               ref("println"),
                                               ref("(Ljava/lang/String;)V"),
                                               false));
    method.instructions.add(new InsnNode(Opcodes::RETURN));

    BasicVerifier        verifier;
    Analyzer<BasicValue> analyzer(&verifier);
    EXPECT_NO_THROW(analyzer.analyze(ref("Hello"), &method));
}

}   // namespace
